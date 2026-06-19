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
#include <locale>
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
void printSortedArrayElement(std::vector<string> elements,
                             const std::string &separator = " ") {
  std::sort(elements.begin(), elements.end());
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

// to do make this function accept a tokenValue instead of all tokens
std::vector<std::string>
fileCompletion(const std::vector<parser::Token> &tokens) {
  if (tokens.size() == 0) {
    return {};
  }
  std::vector<std::string> result{};
  auto lastToken = tokens.back();
  if (lastToken.type != parser::TokenType::WORD) {
    return {};
  }
  const auto currentDirectoryFilesAndDirectories =
      file_helpers::getCurrentDirectoryFilesAndDirectories();
  if (currentDirectoryFilesAndDirectories.empty()) {
    return {};
  }
  for (const auto &e : currentDirectoryFilesAndDirectories) {
    if (tokens.size() == 1) {
      if (e.type == file_helpers::EntryType::Directory) {
        result.push_back(e.name + "/");
      } else {
        result.push_back(e.name + " ");
      }
      continue;
    }
    if (lastToken.value.front() == e.name.front() &&
        e.name.contains(lastToken.value)) {
      if (e.type == file_helpers::EntryType::Directory) {
        result.push_back(e.name + "/");
      } else {
        result.push_back(e.name + " ");
      }
    }
  }

  return result;
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
      if (file.type == file_helpers::EntryType::Directory) {
        result.push_back(path + file.name + "/");
      } else {
        result.push_back(path + file.name + " ");
      }
    } else if (fileToCompleteName.front() == file.name.front() &&
               file.name.contains(fileToCompleteName)) {
      if (file.type == file_helpers::EntryType::Directory) {
        result.push_back(path + file.name + "/");
      } else {
        result.push_back(path + file.name + " ");
      }
    }
  }

  return result;
}
void printBuffer(const std::string &buffer) {
  const std::string prompt = "$ ";
  std::cout << "\r" << prompt << buffer;
  std::cout.flush();
}
std::string getBufferFromTokens(const std::vector<parser::Token> &tokens) {
  // here we skip the last element
  std::string result = "";
  for (size_t i{0}; i < tokens.size() - 1; ++i) {
    if (tokens.at(i).type != parser::TokenType::WORD) {
      break;
    }
    if (i != 0) {
      result += " ";
    }
    result += tokens.at(i).value;
  }
  return result;
}
std::string readUserInputWithAutoComplete() {
  static std::vector<std::string> history;
  static int historyIndex = 0;

  constexpr char tab = '\t';
  bool isSecondTab = true;

  Terminal terminal;
  terminal.enableRaw();

  std::string buffer;
  size_t cursor = 0;

  char c;
  const std::string prompt = "$ ";

  bool tabPressedBefore = false;
  std::string lastBuffer;
  std::vector<std::string> lastCompletions;

  auto redraw = [&]() {
    std::cout << "\r" << prompt;
    std::cout << buffer;
    std::cout << "\033[K";

    size_t pos = prompt.size() + cursor;
    size_t total = prompt.size() + buffer.size();

    if (total > pos)
      std::cout << "\033[" << (total - pos) << "D";

    std::cout.flush();
  };
  auto bell = []() { std::cout << "\a"; };

  std::cout << prompt << std::flush;

  while (true) {
    read(STDIN_FILENO, &c, 1);

    // ================= ESC (ARROWS + HISTORY) =================
    if (c == 27) {
      char seq[2];
      read(STDIN_FILENO, &seq[0], 1);
      read(STDIN_FILENO, &seq[1], 1);

      if (seq[0] == '[') {
        if (seq[1] == 'A') // UP
        //
        {
          if (!history.empty() && historyIndex > 0)
            historyIndex--;

          if (!history.empty())
            buffer = history[historyIndex];

          cursor = buffer.size();
          redraw();
        } else if (seq[1] == 'B') // DOWN
        {
          if (!history.empty() && historyIndex < (int)history.size() - 1)
            historyIndex++;
          else
            historyIndex = history.size();

          buffer = (historyIndex >= (int)history.size())
                       ? ""
                       : history[historyIndex];

          cursor = buffer.size();
          redraw();
        } else if (seq[1] == 'C') {
          if (cursor < buffer.size())
            cursor++;
          redraw();
        } else if (seq[1] == 'D') {
          if (cursor > 0)
            cursor--;
          redraw();
        }
      }

      continue;
    }

    // ================= ENTER =================
    if (c == '\n') {
      std::cout << "\n";

      if (!buffer.empty()) {
        history.push_back(buffer);
        historyIndex = history.size();
      }

      break;
    }

    // ================= BACKSPACE =================
    if (c == 127) {
      if (cursor > 0) {
        buffer.erase(cursor - 1, 1);
        cursor--;
        redraw();
      }

      tabPressedBefore = false;
      lastBuffer.clear();
      lastCompletions.clear();
      continue;
    }

    // ================= TAB (FULL MERGED COMPLETION) =================
    if (c == tab) {
      isSecondTab = !isSecondTab;
      auto tokens = parser1.lex(str::Trim(buffer));

      std::vector<std::string> completions;

      // ----------   CUSTOMER COMPLETION -----------
      if (!tokens.empty() && (buffer.back() == ' ' || tokens.size() > 1)) {
        auto [args, noNeeed] = parser1.joinWords(tokens);

        auto result = executer.customCompletion(args);
        if (result.has_value()) {
          if (args.size() == 1) {
            buffer = args.front() + " " + result.value();
          } else {
            std::vector<std::string> temp(args.begin(), args.end() - 1);
            buffer = str::JoinString(temp, " ");
            buffer += (" " + result.value());
          }
          cursor = buffer.size();
          redraw();
          continue;
        }
        bell();
      }

      // ---------- FILE / PATH COMPLETION ----------
      if (!tokens.empty() &&
          (tokens.size() > 1 || (!buffer.empty() && buffer.back() == ' '))) {
        if (tokens.back().type != parser::TokenType::WORD)
          continue;

        if (tokens.back().value.find("/") != std::string::npos)
          completions = nestedfileCompletion(tokens.back().value);
        else
          completions = fileCompletion(tokens);

        if (completions.empty()) {
          std::cout << "\a";
          continue;
        }
        if (completions.size() == 1) {

          if (tokens.size() < 2) {

            buffer = tokens.front().value + " " + completions.front();
          } else {

            buffer = getBufferFromTokens(tokens) + " " + completions.front();
          }
          cursor = buffer.size();
          redraw();

          continue;
        }

        // for zaki : to understand this code you should know about completions
        // and partial completions with LCP(longet common prefix)
        // and the diffrence between press tab once and twice

        bool same = tabPressedBefore && buffer == lastBuffer &&
                    completions == lastCompletions;

        // second TAB → list
        if (same) {
          std::cout << "\n";
          printSortedArrayElement(completions, "  ");
          std::cout << "\n";
          redraw();

          tabPressedBefore = false;
          continue;
        }

        // first TAB → LCP
        std::string prefix = longestCommonPrefix(completions);

        auto bufferTokens = parser1.lex(buffer);

        if (prefix.size() > bufferTokens.back().value.size()) {

          if (bufferTokens.size() == 2) {
            buffer = bufferTokens.front().value + " " + prefix;
          } else {
            buffer = getBufferFromTokens(tokens) + " " + prefix;
          }
          cursor = buffer.size();
          redraw();
        } else {
          std::cout << "\a";
        }

        lastBuffer = buffer;
        lastCompletions = completions;
        tabPressedBefore = true;

        continue;
      }

      // ---------- BUILTIN ----------
      std::string builtin = builtInAutoComplete(buffer);

      if (builtin != buffer) {
        buffer = builtin;
        cursor = buffer.size();
        redraw();
        continue;
      }

      // ---------- PATH EXECUTABLES ----------
      completions = notBuiltInutoComplete(buffer);

      if (completions.empty()) {
        std::cout << "\a";
        continue;
      }

      // single match
      if (completions.size() == 1) {
        buffer = completions.front() + " ";
        cursor = buffer.size();
        redraw();
        continue;
      }

      bool same = tabPressedBefore && buffer == lastBuffer &&
                  completions == lastCompletions;

      // second TAB → list
      if (same) {
        std::cout << "\n";
        printArrayElement(completions, "  ");
        std::cout << "\n";
        redraw();

        tabPressedBefore = false;
        continue;
      }

      // first TAB → LCP
      std::string prefix = longestCommonPrefix(completions);

      if (prefix.size() > buffer.size()) {
        buffer = prefix;
        cursor = buffer.size();
        redraw();
      } else {
        std::cout << "\a";
      }

      lastBuffer = buffer;
      lastCompletions = completions;
      tabPressedBefore = true;

      continue;
    }

    // ================= NORMAL CHAR =================
    buffer.insert(buffer.begin() + cursor, c);
    cursor++;
    redraw();

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
    input = str::Trim(input);
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
