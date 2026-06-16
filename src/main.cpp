#include "Executor.hpp"
#include "Parser.hpp"
#include "Terminal.hpp"
#include "helpers/file_helpers.hpp"
#include "str.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <termios.h>
#include <unistd.h>

using namespace std;
parser::Parser parser1;
Executor executer;

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
        result.push_back(file);
      }
    }
  }
  std::sort(result.begin(), result.end(),
            [](const std::string &a, const std::string &b) {
              if (a.size() != b.size())
                return a.size() < b.size(); // shorter first
              return a < b;                 // lexicographic tie-break
            });
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
void printCompletedCommand(const std::string &command) {
  const std::string prompt = "$ ";
  // redraw clean line
  std::cout << "\r" << prompt;

  std::cout << command;

  // IMPORTANT: erase leftover chars from previous longer input
  std::cout << " \r";
  std::cout << "\r" << prompt << command;

  std::cout.flush();
}

std::string longestCommonPrefix(const std::vector<std::string> &elements) {
  if (elements.empty())
    return "";

  std::string prefix = elements.front();

  for (size_t i = 1; i < elements.size(); i++) {

    size_t j = 0;

    while (j < prefix.size() && j < elements[i].size() &&
           prefix[j] == elements[i][j]) {
      j++;
    }

    prefix = prefix.substr(0, j);
  }

  return prefix;
}

std::optional<std::string>
fileCompletion(const std::vector<parser::Token> &tokens) {
  if (tokens.size() == 1) {
    return std::nullopt;
  }
  auto lastToken = tokens.back();
  if (lastToken.type != parser::TokenType::WORD) {
    return std::nullopt;
  }
  const auto currentDirectoryFiles = file_helpers::getCurrentDirectoryFiles();
  if (currentDirectoryFiles.empty()) {
    return std::nullopt;
  }
  auto firstFile = currentDirectoryFiles.front();
  if (lastToken.value.front() == firstFile.front() &&
      firstFile.contains(lastToken.value)) {
    return firstFile;
  }
  return std::nullopt;
}
std::vector<std::string> nestedfileCompletion(const std::string &tokenValue) {
  std::string path = tokenValue;
  std::string fileToCompleteName = "";
  std::vector<std::string> result{};
  for (auto it = tokenValue.rbegin(); it != tokenValue.rend(); ++it) {
    // me/bilal/fi
    if (*it == '/') {
      break;
    } else {
      fileToCompleteName.insert(fileToCompleteName.begin(), *it);
      path.pop_back();
    }
  }

  auto files = file_helpers::getDirectoryFiles(path);
  if (files.empty()) {
    return {};
  }
  for (const auto &file : files) {
    if (path == tokenValue) {
      result.push_back(path + file);
    } else if (fileToCompleteName.front() == file.front() &&
               file.contains(fileToCompleteName)) {
      result.push_back(path + file);
    }
  }

  return result;
}
void printBuffer(const std::string &buffer) {
  const std::string prompt = "$ ";
  std::cout << "\r" << prompt << buffer;
  std::cout.flush();
}
std::string readUserInputWithAutoComplete() {

  Terminal terminal;
  terminal.enableRaw();

  std::string buffer;
  char c;

  const std::string prompt = "$ ";

  bool tabPressedBefore = false;
  std::string lastBuffer = "";
  std::vector<std::string> lastCompletions{};

  std::cout << prompt;
  std::cout.flush();

  while (true) {

    read(STDIN_FILENO, &c, 1);

    // ENTER
    if (c == '\n') {
      std::cout << "\n";
      break;
    }

    // TAB
    if (c == '\t') {
      auto tokens = parser1.lex(buffer);
      std::optional<string> file = std::nullopt;
      if (tokens.size() > 1) {
        if (tokens.back().type != parser::TokenType::WORD) {
          continue;
        }
        if (tokens.back().value.contains("/")) {
          auto temp = nestedfileCompletion(tokens.back().value);
          if (temp.empty()) {
            continue;
          }
          file = temp.front();

        } else {
          file = fileCompletion(tokens);
        }
        if (!file.has_value()) {
          continue;
        }

        buffer = tokens.front().value + " " + file.value() + " ";
        printBuffer(buffer);
        continue;
      }

      std::string temp = builtInAutoComplete(buffer);

      // builtin completion
      if (temp != buffer) {

        buffer = temp;

        std::cout << "\r" << prompt << buffer;
        std::cout.flush();

        tabPressedBefore = false;
        lastBuffer.clear();
        lastCompletions.clear();

        continue;
      }

      auto completions = notBuiltInutoComplete(buffer);

      if (completions.empty()) {

        std::cout << "\a";
        std::cout.flush();

        tabPressedBefore = false;
        continue;
      }

      // one match
      if (completions.size() == 1) {

        buffer = completions.front() + " ";

        std::cout << "\r" << prompt << buffer;
        std::cout.flush();

        tabPressedBefore = false;
        lastBuffer.clear();
        lastCompletions.clear();

        continue;
      }

      bool sameSituation = tabPressedBefore && buffer == lastBuffer &&
                           completions == lastCompletions;

      // second TAB
      if (sameSituation) {

        auto display = completions;

        std::sort(display.begin(), display.end());

        std::cout << "\n";

        printArrayElement(display, "  ");

        std::cout << "\n";

        std::cout << prompt << buffer;
        std::cout.flush();

        tabPressedBefore = false;

        continue;
      }

      // first TAB -> LCP

      std::string prefix = longestCommonPrefix(completions);

      if (prefix.size() > buffer.size()) {

        buffer = prefix;

        std::cout << "\r" << prompt << buffer;
        std::cout.flush();

      } else {

        // no progress
        std::cout << "\a";
        std::cout.flush();
      }

      lastBuffer = buffer;
      lastCompletions = completions;

      tabPressedBefore = true;

      continue;
    }

    // BACKSPACE
    if (c == 127) {

      if (!buffer.empty()) {

        buffer.pop_back();

        std::cout << "\b \b";
        std::cout.flush();
      }

      tabPressedBefore = false;
      lastBuffer.clear();
      lastCompletions.clear();

      continue;
    }

    // normal char

    buffer.push_back(c);

    std::cout << c;
    std::cout.flush();

    // user typed -> reset tab state
    tabPressedBefore = false;
    lastBuffer.clear();
    lastCompletions.clear();
  }

  terminal.restore();

  return buffer;
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // // // execute();
  bool exit = false;

  while (true) {
    // const std::string input = readUserCommand();
    std::string input = readUserInputWithAutoComplete();
    std::vector<parser::Token> tokens = parser1.lex(input);
    parser::ParsedCommand parsedCommand = parser1.parseInput(tokens);

    executer.run(parsedCommand, exit);
    if (exit) {
      std::cout << endl << "---------------------------------" << endl;
      std::cout << "good by";
      std::cout << endl << "---------------------------------" << endl;
      break;
    }
  }
}
