#include "reconfigure.h"

namespace cli {

Reconfigure::Reconfigure() {
}

void Reconfigure::Parse(int argc, char* argv[]) {
  static struct option long_options[] = {
    {"cluster", required_argument, NULL, 'c'},
    {"help", no_argument, NULL, 'h'},
  };

  std::vector<std::string> old_addresses;
  std::vector<std::string> new_addresses;
  while (true) {
    int c = getopt_long(argc, argv, "c:h", long_options, NULL);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'c':
        old_addresses = SplitCommaSeparated(optarg);
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
  new_addresses = SplitCommaSeparated(argv[optind]);

  SetConfiguration(old_addresses, new_addresses);
}

void Reconfigure::Help() {
}

void Reconfigure::SetConfiguration(
    std::vector<std::string> old_addresses,
    std::vector<std::string> new_addresses) {
  std::cout << "Attempting to modify cluster configuration...\n";
  raft::LeaderProxy proxy(old_addresses);
  protocol::raft::GetConfiguration_Response get_reply;
  auto status = proxy.GetClusterConfiguration(get_reply);
  std::cout << "Existing configuration: ";
  for (auto server:get_reply.servers()) {
    std::cout << server.address() << " ";
  }

  std::vector<protocol::log::Server> servers;
  std::cout << "\nNew configuration: ";
  for (auto address:new_addresses) {
    std::cout << address << " ";
    protocol::log::Server server;
    server.set_address(address);
    servers.push_back(server);
  }
  std::cout << "\n";

  protocol::raft::SetConfiguration_Response set_reply;
  status = proxy.SetClusterConfiguration(get_reply.id(), servers, set_reply);
  std::cout << "Membership result OK? " << (status.ok() ? "Yes" : "No") << "\n";
  if (!status.ok()) {
    std::cout << "Membership change error: " << status.error_message() << "\n";
  }
}

}

