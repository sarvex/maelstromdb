#include "create.h"

namespace cli {

Create::Create() {
}

void Create::Parse(int argc, char* argv[]) {
  static struct option long_options[] = {
    {"leader", no_argument, NULL, 'l'},
    {"help", no_argument, NULL, 'h'},
  };

  std::string address;
  bool initialize_as_leader = false;
  while (true) {
    int c = getopt_long(argc, argv, "c:lh", long_options, NULL);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'l':
        initialize_as_leader = true;
        break;
      case 'h':
        Help();
        exit(0);
      default:
        std::cerr << "Invalid option provided " << c << "\n";
        Help();
        exit(1);
    }
  }

  optind++;
  if (optind == argc) {
    std::cerr << "Expected additional argument to be provided\n";
    Help();
    exit(1);
  }
  address = argv[optind];

  InitializeNode(address, initialize_as_leader);
}

void Create::Help() {
}

void Create::InitializeNode(std::string address, bool leader) {
  std::cout << "Initializing...\n";
  raft::GlobalCtxManager ctx(address);
  ctx.ConcensusInstance()->StateMachineInit();
  if (leader) {
    ctx.ConcensusInstance()->InitializeConfiguration();
  }

  std::thread client_worker = std::thread(&raft::RaftClientImpl::AsyncCompleteRPC, ctx.ClientInstance());

  ctx.ServerInstance()->ServerInit();

  if (client_worker.joinable()) {
    client_worker.join();
  }
  std::cout << "Node terminated\n";
}

}

