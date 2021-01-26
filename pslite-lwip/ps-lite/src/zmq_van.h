/**
 *  Copyright (c) 2015 by Contributors
 */
#ifndef PS_ZMQ_VAN_H_
#define PS_ZMQ_VAN_H_
#include <zmq.h>
#include <stdlib.h>
#include <thread>
#include <string>
#include "ps/internal/van.h"
#if _MSC_VER
#define rand_r(x) rand()
#endif

namespace ps {
/**
 * \brief be smart on freeing recved data
 */
inline void FreeData(void *data, void *hint) {
  if (hint == NULL) {
	if (data) {
    	delete [] static_cast<char*>(data);
		data = NULL;
	}
  } else {
    delete static_cast<SArray<char>*>(hint);
	hint = NULL;
  }
}

class Postoffice;

/**
 * \brief ZMQ based implementation
 */
class ZMQVan : public Van {
 public:
  ZMQVan(Postoffice *po) : Van(po) { }
  virtual ~ZMQVan() { }

 protected:
	int Init(const char *ip, const char *gw, const char *mask) override {
		// init lwip
		return zmq_global_init(ip, gw, mask);
	}

  void Start() override {
    // start zmq
    context_ = zmq_ctx_new();
    CHECK(context_ != NULL) << "create 0mq context failed";
    zmq_ctx_set(context_, ZMQ_MAX_SOCKETS, 65536);
    // zmq_ctx_set(context_, ZMQ_IO_THREADS, 4);
    Van::Start();
  }

  void Stop() override {
    PS_VLOG(1) << my_node_.ShortDebugString() << " is stopping";
    Van::Stop();
    // close sockets
    int linger = 0;
    int rc = zmq_setsockopt(receiver_, ZMQ_LINGER, &linger, sizeof(linger));
    CHECK(rc == 0 || errno == ETERM);
    CHECK_EQ(zmq_close(receiver_), 0);

	if (my_node_.role == Node::SCHEDULER) {
    	for (auto& it : senders_) {
      		int rc = zmq_setsockopt(it.second, ZMQ_LINGER, &linger, sizeof(linger));
      		CHECK(rc == 0 || errno == ETERM);
      		CHECK_EQ(zmq_close(it.second), 0);
    	}
	}
	else {
		int rc = 0;

		if (data_rxtx) {
			rc = zmq_setsockopt(data_rxtx, ZMQ_LINGER, &linger, sizeof(linger));
      		CHECK(rc == 0 || errno == ETERM);
      		CHECK_EQ(zmq_close(data_rxtx), 0);
		}
		if (ctrl_rxtx) {
			rc = zmq_setsockopt(ctrl_rxtx, ZMQ_LINGER, &linger, sizeof(linger));
      		CHECK(rc == 0 || errno == ETERM);
      		CHECK_EQ(zmq_close(ctrl_rxtx), 0);
		}
	}
    zmq_ctx_destroy(context_);
  }

  int Bind(const Node& node, int max_retry) override {
	void *recver = NULL;

    recver = zmq_socket(context_, ZMQ_ROUTER);
    CHECK(recver != NULL)
        << "create receiver socket failed: " << zmq_strerror(errno);
//    int local = GetEnv("DMLC_LOCAL", 0);
	int local = 0;
    std::string hostname = node.hostname.empty() ? "*" : node.hostname;
    std::string addr = local ? "ipc:///tmp/" : "tcp://" + hostname + ":";
    int port = node.port;

	if (node.id != node.kEmpty)
		zmq_set_localid(recver, node.id);

//	fprintf(stdout, "[%s][%d]: addr %s, port %d\n",
//					__FILE__, __LINE__, addr.c_str(), port);
    unsigned seed = static_cast<unsigned>(time(NULL)+port);
    for (int i = 0; i < max_retry+1; ++i) {
      auto address = addr + std::to_string(port);
	  fprintf(stdout, "[%s][%d]: bind to addr %s, retry %d.\n",
					  __FILE__, __LINE__, address.c_str(), i);
      if (zmq_bind(recver, address.c_str()) == 0) break;
      if (i == max_retry) {
        port = -1;
      } else {
        port = 10000 + rand_r(&seed) % 40000;
      }
    }

	receiver_ = recver;
    return port;
  }

  int Connect(const Node &node) override {
  	if (my_node_.role == Node::SCHEDULER)
		return scheduler_connect(node);

	// servers and workers only need to connect ot scheduler and switch
	if (node.role != Node::SCHEDULER &&
					node.role != Node::SWITCH) {
		return 0;
	}

	if (node.role == Node::SCHEDULER && ctrl_rxtx) {
		zmq_close(ctrl_rxtx);
		ctrl_rxtx = NULL;
	}
	else if (node.role == Node::SWITCH && data_rxtx) {
		zmq_close(data_rxtx);
		data_rxtx = NULL;
	}

	void *sock = zmq_socket(context_, ZMQ_DEALER);

	if (!sock) {
		fprintf(stdout, "[%s][%d]: failed to create socket\n",
						__FILE__, __LINE__);
		return -1;
	}

	if (my_node_.id != Node::kEmpty) {
		snprintf(my_id_, 7, "ps%04d", (int)my_node_.id);

		zmq_setsockopt(sock, ZMQ_IDENTITY, my_id_, strlen(my_id_));
		zmq_set_localid(sock, my_node_.id);
	}

	if (node.role == Node::SCHEDULER)
		zmq_set_remoteid(sock, node.id);
    // connect
    std::string addr = "tcp://" + node.hostname + ":" + std::to_string(node.port);
    if (zmq_connect(sock, addr.c_str()) != 0) {
		fprintf(stderr, "[%s][%d]: host %s failed to connect to %s\n",
						__FILE__, __LINE__, my_node_.hostname.c_str(),
						addr.c_str());
		return -1;
    }

	if (node.role == Node::SCHEDULER)
    	ctrl_rxtx = sock;
	else
		data_rxtx = sock;

	return 0;
  }

  int scheduler_connect(const Node& node) {
    CHECK_NE(node.id, node.kEmpty);
    CHECK_NE(node.port, node.kEmpty);
    CHECK(node.hostname.size());

    int id = node.id;
    auto it = senders_.find(id);
    if (it != senders_.end()) {
      zmq_close(it->second);
    }

    void *sender = zmq_socket(context_, ZMQ_DEALER);

	if (!sender) {
		fprintf(stderr, "[%s][%d]: scheduler cannot create socket for node %d\n",
						__FILE__, __LINE__, id);
		return -1;
	}
    if (my_node_.id != Node::kEmpty) {
	  snprintf(my_id_, 7, "ps%04d", (int)my_node_.id);
      zmq_setsockopt(sender, ZMQ_IDENTITY, my_id_, strlen(my_id_));
	  zmq_set_localid(sender, my_node_.id);
	  zmq_set_remoteid(sender, node.id);
	  // TODO set bypass
    }

    // connect
    std::string addr = "tcp://" + node.hostname + ":" + std::to_string(node.port);
    if (zmq_connect(sender, addr.c_str()) != 0) {
		fprintf(stderr, "[%s][%d]: scheduler cannot connect to node %d\n",
						__FILE__, __LINE__, id);
		return -1;
    }
    senders_[id] = sender;
	return 0;
  }

  int SendMsg(const Message& msg) override {
    std::lock_guard<std::mutex> lk(mu_);
    // find the socket
    int id = msg.meta.recver;
    CHECK_NE(id, Meta::kEmpty);

	void *socket = NULL;

	if (my_node_.role == Node::SCHEDULER) {
    	auto it = senders_.find(id);

    	if (it == senders_.end()) {
      		LOG(WARNING) << "there is no socket to node " << id;
      		return -1;
    	}
    	socket = it->second;
	}
	else {
		if (id == kScheduler)
			socket = ctrl_rxtx;
		else {
			if (!is_id_set) {
//				fprintf(stdout, "[%s][%d]: set local id %d\n",
//								__FILE__, __LINE__, my_node_.id);
				zmq_set_localid(data_rxtx, my_node_.id);
				is_id_set = true;
			}

			socket = data_rxtx;
		}

		if (!socket) {
      		LOG(WARNING) << "there is no socket to node " << id;
      		return -1;
		}
	}
//    std::cout << my_node_.role << " Send:"<<(msg.meta.DebugString()) <<std::endl;
    // send meta
    int meta_size; char* meta_buf;
    PackMeta(msg.meta, &meta_buf, &meta_size);
    int tag = 0;
    int n = msg.data.size();
	bool is_hot = msg.meta.is_hot;
    zmq_msg_t meta_msg;

	if (msg.meta.control.empty()) {
		if (strlen(my_id_) > 0) {
			zmq_msg_t id_msg;

//			fprintf(stdout, "[%s][%d]: send identify msg %s\n",
//							__FILE__, __LINE__, my_id_);
			zmq_msg_init_data(&id_msg, my_id_, strlen(my_id_), NULL, NULL, id);

			if (is_hot)
				tag = ZMQ_SNDMORE | ZMQ_DATA | ZMQ_HOT;
			else
				tag = ZMQ_SNDMORE | ZMQ_DATA;
			while (true) {
				if (zmq_msg_send(&id_msg, socket, tag) == strlen(my_id_))
					break;
				if (errno == EINTR) continue;
      			LOG(WARNING) << "failed to send message to node [" << id
						<< "] errno: " << errno << " " << zmq_strerror(errno);
				return -1;
			}
		}

    	zmq_msg_init_data(&meta_msg, meta_buf, meta_size, FreeData, NULL, id);
	}
	else {
    	zmq_msg_init_data(&meta_msg, meta_buf, meta_size, FreeData, NULL);
	}

	if (n > 0) {
		if (is_hot)
			tag = ZMQ_SNDMORE | ZMQ_DATA | ZMQ_HOT;
		else
			tag = ZMQ_SNDMORE | ZMQ_DATA;
	} else if (msg.meta.control.empty()) {
		if (is_hot)
			tag = ZMQ_DATA | ZMQ_HOT;
		else
			tag = ZMQ_DATA;
	} else
		tag = 0;
    while (true) {
      if (zmq_msg_send(&meta_msg, socket, tag) == meta_size) break;
      if (errno == EINTR) continue;
      LOG(WARNING) << "failed to send message to node [" << id
                   << "] errno: " << errno << " " << zmq_strerror(errno);
      return -1;
    }
    zmq_msg_close(&meta_msg);
    int send_bytes = meta_size;

    // send data
    for (int i = 0; i < n; ++i) {
      zmq_msg_t data_msg;
      SArray<char>* data = new SArray<char>(msg.data[i]);
      int data_size = data->size();

      zmq_msg_init_data(&data_msg, data->data(), data->size(), FreeData, data, id);
      if (i == n - 1) tag = ZMQ_DATA;
	  else tag = ZMQ_SNDMORE | ZMQ_DATA;

	  if (i == 0)
			tag |= ZMQ_KEY;

	  if (is_hot)
			tag |= ZMQ_HOT;

//	  fprintf(stdout, "[%s][%d]: send data msg, size %lu, tag %d\n",
//			 			__FILE__, __LINE__, data->size(), tag);

      while (true) {
        if (zmq_msg_send(&data_msg, socket, tag) == data_size) break;
        if (errno == EINTR) continue;
        LOG(WARNING) << "failed to send message to node [" << id
                     << "] errno: " << errno << " " << zmq_strerror(errno)
                     << ". " << i << "/" << n;
        return -1;
      }
      zmq_msg_close(&data_msg);
      send_bytes += data_size;
    }
    return send_bytes;
  }

  int RecvMsg(Message* msg, bool is_data) override {
    msg->data.clear();
    size_t recv_bytes = 0;
	void *recver = NULL;

	if (!is_scheduler_ && is_data)
		recver = data_rxtx;
	else
		recver = receiver_;

    for (int i = 0; ; ++i) {
      zmq_msg_t* zmsg = new zmq_msg_t;
      CHECK(zmq_msg_init(zmsg) == 0) << zmq_strerror(errno);
      while (true) {
        if (zmq_msg_recv(zmsg, recver, 0) != -1) break;
        if (errno == EINTR) continue;
        LOG(WARNING) << "failed to receive message. errno: "
                     << errno << " " << zmq_strerror(errno);
        return -1;
      }
      char* buf = CHECK_NOTNULL((char *)zmq_msg_data(zmsg));
      size_t size = zmq_msg_size(zmsg);
      recv_bytes += size;

      if (i == 0) {
        // identify
        msg->meta.sender = GetNodeID(buf, size);
        msg->meta.recver = my_node_.id;
        CHECK(zmq_msg_more(zmsg));
        zmq_msg_close(zmsg);
		if (zmsg) {
        	delete zmsg;
			zmsg = NULL;
		}
      } else if (i == 1) {
        // task
        UnpackMeta(buf, size, &(msg->meta));
        zmq_msg_close(zmsg);
        bool more = zmq_msg_more(zmsg);

		if (zmsg) {
        	delete zmsg;
			zmsg = NULL;
		}
		
        if (!more) break;
      } else {
        // zero-copy
        SArray<char> data;
        data.reset(buf, size, [zmsg, size](char* buf) {
            zmq_msg_close(zmsg);
			if (zmsg) {
            	delete zmsg;
			}
          });
        msg->data.push_back(data);
        if (!zmq_msg_more(zmsg)) { break; }
      }
    }
    return recv_bytes;
  }

 private:
  /**
   * return the node id given the received identity
   * \return -1 if not find
   */
  int GetNodeID(const char* buf, size_t size) {
    if (size > 2 && buf[0] == 'p' && buf[1] == 's') {
      int id = 0;
      size_t i = 2;
      for (; i < size; ++i) {
        if (buf[i] >= '0' && buf[i] <= '9') {
          id = id * 10 + buf[i] - '0';
        } else {
          break;
        }
      }
      if (i == size) return id;
    }
    return Meta::kEmpty;
  }

  void *context_ = nullptr;
  /**
   * \brief node_id to the socket for sending data to this node
   */
  std::mutex mu_;
  void *receiver_ = nullptr;

  std::unordered_map<int, void*> senders_;

  void *ctrl_rxtx = nullptr;
  bool is_id_set = false;
  void *data_rxtx = nullptr;


};

}  // namespace ps

#endif  // PS_ZMQ_VAN_H_





// monitors the liveness other nodes if this is
// a schedule node, or monitors the liveness of the scheduler otherwise
// aliveness monitor
// CHECK(!zmq_socket_monitor(
//     senders_[kScheduler], "inproc://monitor", ZMQ_EVENT_ALL));
// monitor_thread_ = std::unique_ptr<std::thread>(
//     new std::thread(&Van::Monitoring, this));
// monitor_thread_->detach();

// void Van::Monitoring() {
//   void *s = CHECK_NOTNULL(zmq_socket(context_, ZMQ_PAIR));
//   CHECK(!zmq_connect(s, "inproc://monitor"));
//   while (true) {
//     //  First frame in message contains event number and value
//     zmq_msg_t msg;
//     zmq_msg_init(&msg);
//     if (zmq_msg_recv(&msg, s, 0) == -1) {
//       if (errno == EINTR) continue;
//       break;
//     }
//     uint8_t *data = static_cast<uint8_t*>(zmq_msg_data(&msg));
//     int event = *reinterpret_cast<uint16_t*>(data);
//     // int value = *(uint32_t *)(data + 2);

//     // Second frame in message contains event address. it's just the router's
//     // address. no help

//     if (event == ZMQ_EVENT_DISCONNECTED) {
//       if (!is_scheduler_) {
//         PS_VLOG(1) << my_node_.ShortDebugString() << ": scheduler is dead. exit.";
//         exit(-1);
//       }
//     }
//     if (event == ZMQ_EVENT_MONITOR_STOPPED) {
//       break;
//     }
//   }
//   zmq_close(s);
// }
