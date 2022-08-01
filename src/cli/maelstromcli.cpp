#include <iostream>
#include <string>

#include "logger.h"
#include "create.h"
#include "reconfigure.h"

namespace cli {

class MaelstromCLI {
public:
  MaelstromCLI() {};

  void Execute(int argc, char* argv[]) {
    if (argc <= 1 || argv[argc - 1] == NULL) {
      std::cerr << "No command provided\n";
      CommandList();
    }

    std::string command = argv[1];
    if (command == "create") {
      auto parser = Create();
      parser.Parse(argc, argv);
    } else if (command == "reconfigure") {
      auto parser = Reconfigure();
      parser.Parse(argc, argv);
    } else if (command == "query") {
    } else if (command == "write") {
    } else if (command == "help") {
      CommandList();
    } else {
      std::cerr << "Command provided is not valid\n";
      CommandList();
    }
  }

private:
  void CommandList() {
  }
};

}

int main(int argc, char* argv[]) {
  Logger::SetLevel(Logger::LogLevel::DEBUG);
  Logger::SetLogConsole();
  cli::MaelstromCLI cli;

  cli.Execute(argc, argv);

  return 0;
}

