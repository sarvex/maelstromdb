#ifndef QUERY_H
#define QUERY_H

#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

#include "command_parser.h"
#include "leader_proxy.h"

namespace cli {

class Query : public CommandParser {
public:
  Query();

  void Parse(int argc, char* argv[]) override;

  void Help() override;

private:
  void Execute(std::vector<std::string>& addresses, std::string command);
};

}

#endif

