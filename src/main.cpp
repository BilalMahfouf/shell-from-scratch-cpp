#include <array>
#include <iostream>
#include <string>

std::string readUserCommand() {
  std::string command = "";
  std::cout << "$ ";
  std::getline(std::cin, command);
  return command;
}

const std::string ECHO = "echo";
const std::string TYPE = "type";

const std::array<std::string, 3> BUIT_IN_TYPES = {ECHO, TYPE, "exit"};

void printInvalidCommand(const std::string &command) {
  std::cout << command << ": not found \n";
}

void echo(const std::string &message) { std::cout << message << "\n"; }

void type(const std::string &type) {
  bool isBuiltInCommand = false;
  for (auto item : BUIT_IN_TYPES) {
    if (type == item) {
      std::cout << type << " is a shell builtin \n";
      isBuiltInCommand = true;
      break;
    } else {
      isBuiltInCommand = false;
    }
  }
  if (!isBuiltInCommand) {
    printInvalidCommand(type);
  }
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    const std::string input = readUserCommand();

    if (input == "exit") {
      break;
    }
    std::string message = "";
    std::string command = input.substr(0, ECHO.size());
    if (command == ECHO) {
      message = input.substr(ECHO.size() + 1, input.size());
      echo(message);
    } else if (command == TYPE) {

      message = input.substr(TYPE.size() + 1);
      type(message);
    } else {
      printInvalidCommand(input);
    }
  }
}
