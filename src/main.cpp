#include <iostream>
#include <string>

std::string readUserCommand() {

  std::string message = "";
  std::cout << "$ ";
  std::cin >> message;
  return message;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  const std::string command = readUserCommand();

  std::cout << command << ": command not found";
}
