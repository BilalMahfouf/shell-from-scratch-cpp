#include "./tests/parser_tests.hpp"
#include "Executor.hpp"
#include "Parser.hpp"
#include "helpers/file_helpers.hpp"
#include "str.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <execution>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <termios.h>
#include <unistd.h>

using namespace std;

class Terminal {
private:
  termios original;

public:
  void restore() { tcsetattr(STDIN_FILENO, TCSANOW, &original); }
  void enableRaw() {
    tcgetattr(STDIN_FILENO, &original);

    termios raw = original;
    raw.c_lflag &= ~(ICANON | ECHO);

    // apply new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  }
};
std::string readUserCommand() {
  std::string command = "";
  std::cout << "$ ";
  std::getline(std::cin, command);
  return command;
}
std::string builtInAutoComplete(const std::string &input) {
  if (input == "ech") {
    return input + "o ";
  }
  if (input == "exi") {
    return input + "t ";
  }
  if (input == "typ") {
    return input + "e ";
  }
  if (input == "echo" || input == "exit" || input == "type") {
    return input + " ";
  }
  return input;
}

std::vector<string> notBuiltInutoComplete(const std::string &input) {
  const char *env = getenv("PATH");
  if (!env) {
    return {};
  }
  std::string path = env;
  std::vector<string> result{};

  auto paths = str::Split(path, ":");
  for (const auto &dir : paths) {
    if (!fs::exists(dir) || !fs::is_directory(dir))
      continue;

    for (const auto entry : fs::directory_iterator(dir)) {
      const auto file = entry.path().filename().string();
      if (file.starts_with(input) && access(entry.path().c_str(), X_OK) == 0) {
        result.push_back(file + " ");
      }
    }
  }
  return result;
}

void printArrayElement(const std::vector<string> &elements,
                       const std::string &separator = " ") {
  for (size_t i{0}; i < elements.size(); ++i) {
    if (i != 0) {
      std::cout << separator;
    }
    std::cout << elements.at(i);
  }
}
std::string readUserInputWithAutoComplete() {
  Terminal terminal;
  terminal.enableRaw();
  std::string buffer;
  char c;
  const std::string prompt = "$ ";

  std::cout << prompt;
  std::cout.flush();

  while (true) {

    read(STDIN_FILENO, &c, 1);

    // ENTER
    if (c == '\n') {
      std::cout << "\n";
      break;
    }

    // TAB (autocomplete)
    if (c == '\t') {

      std::string temp = builtInAutoComplete(buffer);
      if (temp == buffer) {
        const auto completions = notBuiltInutoComplete(buffer);

        std::cout << "\a";
        if (completions.size() > 1) {
          std::cout << std::endl;
          printArrayElement(completions, " ");
          std::cout << endl;
          // redraw clean line
          std::cout << "\r" << prompt;

          std::cout << buffer;

          continue;
        }

        if (completions.empty()) {
          continue;
        }

        if (completions.front() == buffer) {
          continue;
        }
        temp = completions.front();
      }
      buffer = temp;

      // redraw clean line
      std::cout << "\r" << prompt;

      std::cout << buffer;

      // IMPORTANT: erase leftover chars from previous longer input
      std::cout << " \r";
      std::cout << "\r" << prompt << buffer;

      std::cout.flush();
      continue;
    }

    // BACKSPACE
    if (c == 127) {
      if (!buffer.empty()) {
        buffer.pop_back();

        std::cout << "\b \b";
        std::cout.flush();
      }
      continue;
    }

    // normal char
    buffer.push_back(c);
    std::cout << c;
    std::cout.flush();
  }

  terminal.restore();
  return buffer;
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // // // execute();
  parser::Parser parser;
  Executor executer;
  bool exit = false;

  while (true) {
    // const std::string input = readUserCommand();
    std::string input = readUserInputWithAutoComplete();
    std::vector<parser::Token> tokens = parser.lex(input);
    parser::ParsedCommand parsedCommand = parser.parseInput(tokens);

    executer.run(parsedCommand, exit);
    if (exit) {
      std::cout << endl << "---------------------------------" << endl;
      std::cout << "good by";
      std::cout << endl << "---------------------------------" << endl;
      break;
    }
  }
}
