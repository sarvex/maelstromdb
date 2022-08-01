#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <sstream>
#include <string>
#include <vector>

namespace cli {

class CommandParser {
public:
  CommandParser();
  virtual ~CommandParser();

  virtual void Parse(int argc, char* argv[]) = 0;

  virtual void Help() = 0;

protected:
  static std::vector<std::string> SplitCommaSeparated(std::string s); 
};

}

#endif

