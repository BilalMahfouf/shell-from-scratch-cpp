#include <iostream>
#include <string>

std::string readUserCommand() {
  std::string command = "";
  std::cout << "$ ";
  std::getline(std::cin, command);
  return command;
}

const std::string ECHO = "echo";

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    const std::string command = readUserCommand();

    if (command == "exit") {
      break;
    }

    std::string echo = command.substr(0, ECHO.size());
    if (echo == ECHO) {
      std::string prompt = command.substr(ECHO.size() + 1, command.size());
      std::cout << prompt << "\n";
    } else {
      std::cout << command << ": command not found \n";
    }
  }
}
