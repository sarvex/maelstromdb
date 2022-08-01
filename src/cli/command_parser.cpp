#include "command_parser.h"

namespace cli {

CommandParser::CommandParser() {
}

CommandParser::~CommandParser() {
}

std::vector<std::string> CommandParser::SplitCommaSeparated(std::string s) {
  std::stringstream ss(s);
  std::vector<std::string> args;
  while (ss.good()) {
    std::string curr;
    getline(ss, curr, ',');
    args.push_back(curr);
  }
  return args;
}

}

