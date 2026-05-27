#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;

namespace fs = std::filesystem;

std::string readUserCommand() {
  std::string command = "";
  std::cout << "$ ";
  std::getline(std::cin, command);
  return command;
}
bool isNullOrWhiteSpace(const std::string &s) {
  return std::all_of(s.begin(), s.end(),
                     [](unsigned char c) { return std::isspace(c); }) ||
         s.empty();
}

const std::string ECHO = "echo";
const std::string TYPE = "type";
const std::string EXIT = "exit";

enum class Command { Exit = 0, Echo, Type, Pwd, Cd, None };

Command getEnumCommand(const string &str) {
  if (str == "exit")
    return Command::Exit;
  if (str == "echo")
    return Command::Echo;
  if (str == "type")
    return Command::Type;
  if (str == "pwd")
    return Command::Pwd;
  if (str == "cd")
    return Command::Cd;

  return Command::None;
}
std::string getStringCommand(const Command &command) {
  switch (command) {
  case Command::Echo:
    return "echo";
  case Command::Exit:
    return "exit";
  case Command::Type:
    return "type";
  case Command::Pwd:
    return "pwd";
  case Command::Cd:
    return "cd";
  }
  return "";
}

const std::array<std::string, 3> BUIT_IN_TYPES = {ECHO, TYPE, EXIT};

void printInvalidCommand(const std::string &command) {
  std::cout << command << ": not found \n";
}

void echo(const std::string &message) { std::cout << message << endl; }

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
    start = end + 1;
  }
  return "";
}

void type(const std::string &type) {
  const Command command = getEnumCommand(type);
  if (command == Command::None) {
    string path = getExecutableCommandPath(type);
    if (path.empty()) {
      printInvalidCommand(type);
    } else {
      cout << type << " is " << path << endl;
    }
  } else {
    std::cout << type << " is a shell builtin" << endl;
  }
}

std::vector<string> split(const string &str, const char &separator) {
  std::stringstream ss(str);
  std::string temp = "";
  std::vector<string> splitedString = {};
  while (std::getline(ss, temp, separator)) {
    splitedString.push_back(temp);
  }
  return splitedString;
}
void runProgram(const std::string &input) {
  const std::string command = split(input, ' ').front();
  const std::string commandPath = getExecutableCommandPath(command);
  if (commandPath == "") {
    printInvalidCommand(command);
    return;
  }
  std::system(input.c_str());
}

std::string getCurrentWorkingDirectory() {
  try {
    fs::path currentDir = fs::current_path();
    return currentDir.string();
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  return "";
}
void cd(const std::string &absolutePath) {
  if (isNullOrWhiteSpace(absolutePath))
    return;
  if (absolutePath.at(0) == '/' || absolutePath.at(0) == '~') {
    try {
      fs::current_path(absolutePath);
      return;
    } catch (const fs::filesystem_error &e) {
      std::cout << "cd: " << absolutePath << ": No such file or directory"
                << endl;
      // std::cerr << "Error: " << e.what() << std::endl;
    }
  } else {
    std::cerr << "Please provide an absolute path " << endl;
  }
}
void execute() {
  while (true) {
    const std::string input = readUserCommand();
    const Command command = getEnumCommand(split(input, ' ').front());
    std::string message = "";
    switch (command) {
    case Command::Exit:
      return;
    case Command::Echo:
      message = input.substr(getStringCommand(Command::Echo).size() + 1);
      echo(message);
      break;
    case Command::Type:
      message = input.substr(getStringCommand(Command::Type).size() + 1);
      type(message);
      break;
    case Command::Pwd: {
      const std::string currentPath = getCurrentWorkingDirectory();
      if (!isNullOrWhiteSpace(currentPath)) {
        std::cout << currentPath << endl;
      }
      break;
    }
    case Command::Cd:
      message = input.substr(getStringCommand(Command::Cd).size() + 1);
      cd(message);
      break;
    case Command::None:
      runProgram(input);
      break;
    }
  }
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  execute();
}
