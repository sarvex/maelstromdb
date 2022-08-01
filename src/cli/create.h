#ifndef CREATE_H
#define CREATE_H

#include <getopt.h>
#include <iostream>
#include <string>

#include "command_parser.h"
#include "global_ctx_manager.h"
#include "raft_client.h"
#include "raft_server.h"

namespace cli {

class Create : public CommandParser {
public:
  Create();

  void Parse(int argc, char* argv[]) override;

  void Help() override;

private:
  void InitializeNode(std::string address, bool leader);
};

}

#endif

