#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

using namespace std;

namespace fs = std::filesystem;

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

bool isExecutable(const fs::path &p) {
  fs::file_status s = fs::status(p);
  auto perms = s.permissions();

  // Check if any execute bit is set (owner, group, or others)
  return (perms & fs::perms::owner_exec) != fs::perms::none ||
         (perms & fs::perms::group_exec) != fs::perms::none ||
         (perms & fs::perms::others_exec) != fs::perms::none;
}

string getExecutableCommandPath(const std::string &command) {
  const char *env = std::getenv("PATH");
  string path = env;
  size_t start = 0;

  while (true) {
    size_t end = path.find(':', start);
    string dir = path.substr(start, end - start);

    if (end == string::npos) {
      break;
    }

    filesystem::path fullPath = filesystem::path(dir) / command;

    if (filesystem::exists(fullPath) && filesystem::is_regular_file(fullPath) &&
        isExecutable(fullPath)) {
      return fullPath.string();
    }
    ++start;
  }
  return "";
}

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
    string path = getExecutableCommandPath(type);
    if (path.empty()) {
      printInvalidCommand(type);
    } else {
      cout << type << " is " << path << endl;
    }
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
