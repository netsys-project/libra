/**
 *  Copyright (c) 2015 by Contributors
 */
#include <unistd.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <sys/syscall.h>
#include "ps/internal/postoffice.h"
#include "ps/internal/message.h"
#include "ps/base.h"

#include "src/model/lr/lr_worker.h"
#include "src/model/fm/fm_worker.h"
#include "src/model/mvm/mvm_worker.h"
#include "src/model/server.h"

namespace ps {

void Postctl::InsertTask(unsigned task, unsigned allow_dup)
{
	std::vector<Posttask*>::iterator iter;
	Posttask *t;

	for (iter = tasklist.begin(); iter < tasklist.end(); iter++) {
		t = *iter;
		if (t->role < task)
			continue;
		if (t->role == task) {
			if (allow_dup)
				tasklist.insert(iter, new Posttask(
								num_servers_, num_workers_,
								task, verbose_));
			else
				return;
		}
		if (t->role > task) {
			tasklist.insert(iter, new Posttask(
							num_servers_, num_workers_,
							task, verbose_));
			return;
		}
	}
	if (iter == tasklist.end())
		tasklist.push_back(new Posttask(num_servers_,
								num_workers_, task,
								verbose_));
}

int is_scheduler_ready = 1, is_server_ready = 1;
int is_worker_exit = 1, is_server_exit = 1;

void Postctl::GetTaskList(const char *str)
{
	char *taskstr = NULL, *ptok = NULL;

	if (!str)
		return;

	taskstr = strdup(str);
	ptok = strtok(taskstr, ",");
	while (ptok != NULL) {
//		std::cout << ptok << std::endl;

		if (strcmp(ptok, "scheduler") == 0) {
			InsertTask(TASK_SCHEDULER, 0);
			is_scheduler_ready = 0;
//			is_server_ready = 0;
//			is_scheduler_ = 1;
		} else if (strcmp(ptok, "server") == 0) {
			InsertTask(TASK_SERVER, 0);
			is_server_ready = 0;
			is_server_exit = 0;
			local_servers_ ++;
//			is_server_ = 1;
		} else if (strcmp(ptok, "worker") == 0) {
			InsertTask(TASK_WORKER, 1);
			local_workers_++;
			is_worker_exit = 0;
		} else {
			fprintf(stderr, "Wrong role: %s\n", ptok);
		}
		ptok = strtok(NULL, ",");
	}

	free(taskstr);
}

Postctl::Postctl() {
  env_ref_ = Environment::_GetSharedRef();
  const char* val = NULL;
  val = CHECK_NOTNULL(Environment::Get()->find("DMLC_NUM_WORKER"));
  num_workers_ = atoi(val);
  val =  CHECK_NOTNULL(Environment::Get()->find("DMLC_NUM_SERVER"));
  num_servers_ = atoi(val);
  val = CHECK_NOTNULL(Environment::Get()->find("DMLC_ROLE"));

  local_workers_ = 0;
  local_servers_ = 0;

  GetTaskList(val);
//  std::string role(val);
//  is_worker_ = role == "worker";
//  is_server_ = role == "server";
//  is_scheduler_ = role == "scheduler";
  verbose_ = GetEnv("PS_VERBOSE", 0);

  pthread_mutex_init(&map_lock, NULL);
}

Postctl::~Postctl() {
	unsigned int i = 0;

	for (i = 0; i < tasklist.size(); i++) {
		delete tasklist[i];
		tasklist[i] = NULL;
	}
	tasklist.clear();
}

int Postctl::init(void) {
	const char *ip, *gw, *mask;

	ip =  CHECK_NOTNULL(Environment::Get()->find("DMLC_PS_LOCAL_URI"));
	gw =  CHECK_NOTNULL(Environment::Get()->find("DMLC_PS_LOCAL_GW"));
	mask =  CHECK_NOTNULL(Environment::Get()->find("DMLC_PS_LOCAL_MASK"));
	if (tasklist[0]->office.van()->Init(ip, gw, mask) < 0) {
		fprintf(stderr, "Failed to init lwip\n");
		return -1;
	}
	return 0;
}


void Postctl::addThread(int id, Postoffice *po)
{
	pthread_mutex_lock(&map_lock);
	taskmap[id] = po;
	pthread_mutex_unlock(&map_lock);
}

static void startWorker(Posttask *task)
{
	if (task->model == '0') {
		xflow::LRWorker* lr_worker = new xflow::LRWorker(
						task->train_data.c_str(), task->test_data.c_str(),
						&(task->office));
		lr_worker->epochs = task->epochs;
//		fprintf(stdout, "[%s][%d]: skip training\n", __FILE__, __LINE__);
//		if (task->hotdata.size() > 0)
//			lr_worker->train(task->hotdata);
//		else
			lr_worker->train();
	}
	else if (task->model == '1') {
		xflow::FMWorker* fm_worker = new xflow::FMWorker(
						task->train_data.c_str(), task->test_data.c_str(),
						&(task->office));
		fm_worker->epochs = task->epochs;
		fm_worker->train();
	}
	else if (task->model == '2') {
		xflow::MVMWorker* mvm_worker = new xflow::MVMWorker(
						task->train_data.c_str(), task->test_data.c_str(),
						&(task->office));
		mvm_worker->epochs = task->epochs;
		mvm_worker->train();
	
	}
	else
		fprintf(stderr, "Unknown model %c\n", task->model);
}

static void *taskthread(void *arg) {
	Posttask *task = (Posttask *)arg;
	int tid = syscall(SYS_gettid);

	task->office.SetThread(tid);
//	fprintf(stdout, "Thread %d, task %u, po %p\n",
//					tid, task->role, (void*)&(task->office));

	if (task->role == TASK_SERVER) {
		xflow::Server *server = new xflow::Server(&(task->office));
	}

	task->office.Start(NULL, true);
	if (task->role == TASK_WORKER)
		startWorker(task);

  	task->office.Barrier(kWorkerGroup + kServerGroup + kScheduler);
	
	
//	fprintf(stdout, "[%d][%s][%d]: start of finalize\n",
//					tid, __FILE__, __LINE__);
	task->office.Finalize(false);

	if (task->role == TASK_WORKER)
		is_worker_exit = 1;
	else if (task->role == TASK_SERVER)
		is_server_exit = 1;

//	fprintf(stdout, "[%d][%s][%d]: End of finalize\n",
//					tid, __FILE__, __LINE__);
	return NULL;
}

int Postctl::startTasks(int argc, char *argv[]) {
	dmlc::InitLogging("ps-lite\0");

	int worker_cnt = 0;
	for (unsigned i = 0; i < tasklist.size(); i++) {
		Posttask *task = tasklist[i];

		if (task->role == TASK_WORKER) {
			task->train_data = std::string(argv[1]);
			task->test_data = std::string(argv[2]);
			task->model = argv[3][0];
			task->epochs = std::atoi(argv[4]);
			if (argc > 5)
				task->hotdata = std::string(argv[5]);
//			task->heapprofile = std::string(argv[5 + worker_cnt]);
//			fprintf(stdout, "[%s][%d]: init worker %d, train %s, test %s, "
//							"model %c, epochs %d, heap %s\n",
//							__FILE__, __LINE__, worker_cnt, task->train_data.c_str(),
//							task->test_data.c_str(), task->model,
//							task->epochs, task->heapprofile.c_str());
			worker_cnt ++;
		}

		if (tasklist.size() == 1)
			(void *)taskthread(task);
		else {
			if (pthread_create(&(task->tid), NULL, taskthread, (void *)task) == -1) {
				fprintf(stderr, "Failed to create task thread\n");
				return -1;
			}
    		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}

	if (tasklist.size() > 1) {
		for (unsigned i = 0; i < tasklist.size(); i++) {
			pthread_join(tasklist[i]->tid, NULL);
//			fprintf(stdout, "[%s][%d]: main thread get thread %lu(%d) exited\n",
//							__FILE__, __LINE__,
//							tasklist[i]->tid, tasklist[i]->office.gettid());
		}
	}
	return 0;
}

Posttask::Posttask(int s, int w, unsigned r, int v)
	: office(s, w, r, v) {
	role = r;
	tid = -1;
}

Postoffice::Postoffice(int servers, int workers, unsigned role, int v) {
	van_ = Van::Create("zmq", this);
	num_servers_ = servers;
	num_workers_ = workers;

	is_scheduler_ = is_worker_ = is_server_ = 0;
	if (role == TASK_SCHEDULER)
		is_scheduler_ = 1;
	else if (role == TASK_SERVER)
		is_server_ = 1;
	else
		is_worker_ = 1;
	verbose_ = v;
}

void Postoffice::Start(const char* argv0, const bool do_barrier) {
//  // init glog
//  if (argv0) {
//    dmlc::InitLogging(argv0);
//  } else {
//    dmlc::InitLogging("ps-lite\0");
//  }

  // init node info.
  for (int i = 0; i < num_workers_; ++i) {
    int id = WorkerRankToID(i);
    for (int g : {id, kWorkerGroup, kWorkerGroup + kServerGroup,
            kWorkerGroup + kScheduler,
            kWorkerGroup + kServerGroup + kScheduler}) {
      node_ids_[g].push_back(id);
    }
  }

  for (int i = 0; i < num_servers_; ++i) {
    int id = ServerRankToID(i);
    for (int g : {id, kServerGroup, kWorkerGroup + kServerGroup,
            kServerGroup + kScheduler,
            kWorkerGroup + kServerGroup + kScheduler}) {
      node_ids_[g].push_back(id);
    }
  }

  for (int g : {kScheduler, kScheduler + kServerGroup + kWorkerGroup,
          kScheduler + kWorkerGroup, kScheduler + kServerGroup}) {
    node_ids_[g].push_back(kScheduler);
  }

  // start van
  van_->Start();
  // record start time
  start_time_ = time(NULL);

  // do a barrier here
  if (do_barrier) Barrier(kWorkerGroup + kServerGroup + kScheduler);
//  fprintf(stdout,"[%d][%s][%d]: end of postoffice start()\n",
//				  tid, __FILE__,__LINE__);
}

void Postoffice::Finalize(const bool do_barrier) {
  if (do_barrier) Barrier(kWorkerGroup + kServerGroup + kScheduler);
  van_->Stop();
  if (exit_callback_) exit_callback_();
}


void Postoffice::AddCustomer(Customer* customer) {
  std::lock_guard<std::mutex> lk(mu_);
  int id = CHECK_NOTNULL(customer)->id();
  CHECK_EQ(customers_.count(id), (size_t)0) << "id " << id << " already exists";
  customers_[id] = customer;
}


void Postoffice::RemoveCustomer(Customer* customer) {
  std::lock_guard<std::mutex> lk(mu_);
  int id = CHECK_NOTNULL(customer)->id();
  customers_.erase(id);
}


Customer* Postoffice::GetCustomer(int id, int timeout) const {
  Customer* obj = nullptr;
  for (int i = 0; i < timeout*1000+1; ++i) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      const auto it = customers_.find(id);
      if (it != customers_.end()) {
        obj = it->second;
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return obj;
}

void Postoffice::Barrier(int node_group) {
  if (GetNodeIDs(node_group).size() <= 1) return;
  auto role = van_->my_node().role;
  if (role == Node::SCHEDULER) {
    CHECK(node_group & kScheduler);
  } else if (role == Node::WORKER) {
    CHECK(node_group & kWorkerGroup);
  } else if (role == Node::SERVER) {
    CHECK(node_group & kServerGroup);
  }

  std::unique_lock<std::mutex> ulk(barrier_mu_);
  barrier_done_ = false;
  Message req;
  req.meta.recver = kScheduler;
  req.meta.request = true;
  req.meta.control.cmd = Control::BARRIER;
  req.meta.control.barrier_group = node_group;
  req.meta.timestamp = van_->GetTimestamp();
  CHECK_GT(van_->Send(req), 0);

  barrier_cond_.wait(ulk, [this] {
      return barrier_done_;
    });
}

const std::vector<Range>& Postoffice::GetServerKeyRanges() {
  if (server_key_ranges_.empty()) {
    for (int i = 0; i < num_servers_; ++i) {
      server_key_ranges_.push_back(Range(
          kMaxKey / num_servers_ * i,
          kMaxKey / num_servers_ * (i+1)));
    }
  }
  return server_key_ranges_;
}

void Postoffice::Manage(const Message& recv) {
  CHECK(!recv.meta.control.empty());
  const auto& ctrl = recv.meta.control;
  if (ctrl.cmd == Control::BARRIER && !recv.meta.request) {
    barrier_mu_.lock();
    barrier_done_ = true;
    barrier_mu_.unlock();
    barrier_cond_.notify_all();
  }
}

std::vector<int> Postoffice::GetDeadNodes(int t) {
  std::vector<int> dead_nodes;
  if (!van_->IsReady() || t == 0) return dead_nodes;

  time_t curr_time = time(NULL);
  const auto& nodes = is_scheduler_
    ? GetNodeIDs(kWorkerGroup + kServerGroup)
    : GetNodeIDs(kScheduler);
  {
    std::lock_guard<std::mutex> lk(heartbeat_mu_);
    for (int r : nodes) {
      auto it = heartbeats_.find(r);
      if ((it == heartbeats_.end() || it->second + t < curr_time)
            && start_time_ + t < curr_time) {
        dead_nodes.push_back(r);
      }
    }
  }
  return dead_nodes;
}
}  // namespace ps
