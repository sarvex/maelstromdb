#include "write.h"

namespace cli {

Write::Write() {
}

void Write::Parse(int argc, char* argv[]) {
  static struct option long_options[] = {
    {"cluster", required_argument, NULL, 'c'},
    {"help", no_argument, NULL, 'h'},
  };

  std::vector<std::string> cluster;
  std::string command;
  while (true) {
    int c = getopt_long(argc, argv, "c:h", long_options, NULL);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'c':
        cluster = SplitCommaSeparated(optarg);
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
  command = argv[optind];

  Execute(cluster, command);
}

void Write::Help() {
}

void Write::Execute(std::vector<std::string> addresses, std::string command) {
  std::cout << "Attempting to create write query...\n";
  raft::LeaderProxy proxy(addresses);
  protocol::raft::RegisterClient_Response session_reply;
  auto status = proxy.RegisterClient(session_reply);

  std::cout << "Session created successfully? " << (status.ok() ? "Yes" : "No") << "\n";
  if (!status.ok()) {
    return;
  }

  int sequence_num = 1;
  protocol::raft::ClientRequest_Response reply;
  status = proxy.ClientRequest(session_reply.clientid(), sequence_num, command, reply);

  std::cout << "Query successful? " << (status.ok() ? "Yes" : "No") << "\n";
  if (status.ok()) {
    std::cout << "Query response: " << reply.response() << "\n";
  } else {
    std::cout << "Query error: " << status.error_message();
  }
}

}

