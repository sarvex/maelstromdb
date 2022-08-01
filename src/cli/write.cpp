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
  auto session = proxy.RegisterClient();

  std::cout << "Session created successfully? " << (session.status() ? "Yes" : "No") << "\n";

  if (!session.status()) {
    return;
  }
  int sequence_num = 1;
  auto result = proxy.ClientRequest(session.clientid(), sequence_num, command);

  std::cout << "Query successful? " << (result.status() ? "Yes" : "No") << "\n";
  std::cout << "Query response: " << result.response() << "\n";
}

}

