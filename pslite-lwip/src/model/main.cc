/*
 * base.h
 * Copyright (C) 2018 wangxiaoshu <2012wxs@gmail.com>
 *
 * Distributed under terms of the MIT license.
 */

//#include "src/model/lr/lr_worker.h"
//#include "src/model/fm/fm_worker.h"
//#include "src/model/mvm/mvm_worker.h"
//#include "src/model/server.h"

#include "ps/ps.h"

#include <vector>
#include "string.h"

int main(int argc, char *argv[]) {
	int req_argv = 5;
	int ret = 0;

//  if (argc != 5) {
//    std::cout << "sh run_ps_local.sh model_index epochs\n";
//    std::cout << "LR model expmple: sh run_ps_local.sh 0 100\n";
//    std::cout << "FM model expmple: sh run_ps_local.sh 1 100\n";
//    std::cout << std::endl;
//  }
	ret = ps::NumTasks();
	if (ret == 0) {
		fprintf(stderr, "No task found, please set DMLC_ROLE\n");
		return 1;
	}

	/* Workers need one extra argument for each. In the original version, it is
	 * defined in environment variable HEAPPROFILE. We move it to commond-line
	 * setup options. */
	ret = ps::LocalWorkers();
	if (argc < req_argv) {
		fprintf(stderr, "Don't have enough arguments. %d arguments are required.",
						req_argv);
		return 1;
	}

	fprintf(stdout, "%d tasks, %d local workers\n", ps::NumTasks(), ret);

	if (ps::Init()) {
		fprintf(stderr, "Failed to init ps-list\n");
		return 1;
	}
	fprintf(stdout, "Initialization finished\n");

	if (ps::Start(argc, argv)) {
		fprintf(stderr, "Failed to start ps-list\n");
		return 1;
	}

//  if (ps::IsWorker()) {
//    int epochs = std::atoi(argv[4]);
//    if (*(argv[3]) == '0') {
//      std::cout << "start LR " << std::endl;
//      xflow::LRWorker* lr_worker = new xflow::LRWorker(argv[1], argv[2]);
//      lr_worker->epochs = epochs;
//      lr_worker->train();
//    }
//    if (*(argv[3]) == '1') {
//      std::cout << "start FM " << std::endl;
//      xflow::FMWorker* fm_worker = new xflow::FMWorker(argv[1], argv[2]);
//      fm_worker->epochs = epochs;
//      fm_worker->train();
//    }
//    if (*(argv[3]) == '2') {
//      std::cout<< "start MVM " << std::endl;
//      xflow::MVMWorker* mvm_worker = new xflow::MVMWorker(argv[1], argv[2]);
//      mvm_worker->epochs = epochs;
//      mvm_worker->train();
//    }
//  }
//  ps::Finalize();
  return 0;
}
