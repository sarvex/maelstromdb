#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

#include "concensus_module.h"
#include "global_ctx_manager.h"
#include "leader_proxy.h"
#include "logger.h"
#include "raft_client.h"
#include "raft_server.h"

std::vector<std::string> comma_separated_string(std::string address_str) {
  std::stringstream ss(address_str);
  std::vector<std::string> peers;
  while (ss.good()) {
    std::string address;
    getline(ss, address, ',');
    peers.push_back(address);
  }
  return peers;
}

void initialize_node(std::string& address, bool leader) {
  Logger::Info("Initializing...");

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

  Logger::Info("Node terminated");
}

void set_configuration(
    std::vector<std::string>& old_addresses,
    std::vector<std::string>& new_addresses) {
  std::cout << "Attempting to modify cluster configuration...\n";

  raft::LeaderProxy proxy(old_addresses);
  auto cluster = proxy.GetClusterConfiguration();
  std::cout << "Existing configuration: ";
  for (auto server:cluster.servers()) {
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

  auto membership_change = proxy.SetClusterConfiguration(cluster.id(), servers);
  std::cout << "Membership result OK? " << (membership_change.ok() ? "Yes" : "No") << "\n";
}

void help() {

}

int main(int argc, char* argv[]) {
  Logger::SetLevel(Logger::LogLevel::DEBUG);
  Logger::SetLogConsole();

  static struct option long_options[] = {
    {"create", required_argument, NULL, 'n'},
    {"cluster", required_argument, NULL, 'c'},
    {"reconfigure", required_argument, NULL, 'r'},
    {"leader", no_argument, NULL, 'l'},
    {"help", no_argument, NULL, 'h'}
  };

  std::string new_address;
  std::vector<std::string> cluster;
  std::vector<std::string> reconfigure;
  bool initialize_as_leader = false;
  while (true) {
    int c = getopt_long(argc, argv, "n:c:r:lh", long_options, NULL);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'n':
        new_address = optarg;
        break;
      case 'c':
        cluster = comma_separated_string(optarg);
        break;
      case 'r':
        reconfigure = comma_separated_string(optarg);
        break;
      case 'l':
        initialize_as_leader = true;
        break;
      case 'h':
        help();
        exit(0);
      default:
        help();
        exit(1);
    }
  }

  if (optind < argc) {
    help();
    exit(1);
  }

  if (cluster.size() > 0 && reconfigure.size() > 0) {
    set_configuration(cluster, reconfigure);
    exit(0);
  }

  if (new_address.size() > 0) {
    initialize_node(new_address, initialize_as_leader);
  }

  return 0;
}

