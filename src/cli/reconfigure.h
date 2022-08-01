#ifndef RECONFIGURE_H
#define RECONFIGURE_H

#include <getopt.h>
#include <string>
#include <vector>

#include "command_parser.h"
#include "leader_proxy.h"

namespace cli {

class Reconfigure : public CommandParser {
public:
  Reconfigure();

  void Parse(int argc, char* argv[]) override;

  void Help() override;

private:
  void SetConfiguration(
      std::vector<std::string> old_addresses,
      std::vector<std::string> new_addresses);
};

}

#endif

