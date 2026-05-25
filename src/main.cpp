#include <iostream>
#include <string>

std::string readUserCommand() {

  std::string command = "";
  std::cout << "$ ";
  std::getline(std::cin, command);
  return command;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    const std::string command = readUserCommand();

    std::cout << command << ": command not found \n";
  }
}
