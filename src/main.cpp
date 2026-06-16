#include "./tests/parser_tests.hpp"
#include "Executor.hpp"
#include "Parser.hpp"
#include "str.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <execution>
#include <filesystem>
#include <iostream>
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
std::string autoComplete(const std::string &input) {
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

std::string readUserInputWithAutoComplete()
{
    static std::vector<std::string> history;
    static int historyIndex = 0;

    Terminal terminal;
    terminal.enableRaw();

    std::string buffer;
    size_t cursor = 0;

    char c;
    const std::string prompt = "$ ";

auto redraw = [&](){
    std::cout << "\r" << prompt;
    std::cout << buffer;
    std::cout << "\033[K"; // clear line after cursor

    // move cursor to correct position
    size_t pos = prompt.size() + cursor;
    size_t total = prompt.size() + buffer.size();

    if (total > pos)
    {
        std::cout << "\033[" << (total - pos) << "D";
    }

    std::cout.flush();
  };
    std::cout << prompt;

    while (true)
    {
        read(STDIN_FILENO, &c, 1);

        // ================= ESC (ARROWS) =================
        if (c == 27)
        {
            char seq[2];
            read(STDIN_FILENO, &seq[0], 1);
            read(STDIN_FILENO, &seq[1], 1);

            if (seq[0] == '[')
            {
                if (seq[1] == 'A') // UP
                {
                    if (!history.empty() && historyIndex > 0)
                        historyIndex--;

                    if (!history.empty())
                        buffer = history[historyIndex];

                    cursor = buffer.size();
                    redraw();
                }
                else if (seq[1] == 'B') // DOWN
                {
                    if (!history.empty() && historyIndex < (int)history.size() - 1)
                        historyIndex++;
                    else
                        historyIndex = history.size();

                    if (historyIndex >= (int)history.size())
                        buffer = "";
                    else
                        buffer = history[historyIndex];

                    cursor = buffer.size();
                    redraw();
                }
                else if (seq[1] == 'C') // RIGHT
                {
                    if (cursor < buffer.size())
                        cursor++;
                    redraw();
                }
                else if (seq[1] == 'D') // LEFT
                {
                    if (cursor > 0)
                        cursor--;
                    redraw();
                }
            }

            continue;
        }

        // ================= ENTER =================
        if (c == '\n')
        {
            std::cout << "\n";

            if (!buffer.empty())
            {
                history.push_back(buffer);
                historyIndex = history.size();
            }

            break;
        }

        // ================= TAB =================
        if (c == '\t')
        {
            std::string temp = autoComplete(buffer);

            if (temp != buffer)
            {
                buffer = temp;
                cursor = buffer.size();
                redraw();
            }
            else
            {
                std::cout << "\a";
            }
            continue;
        }

        // ================= BACKSPACE =================
        if (c == 127)
        {
            if (cursor > 0)
            {
                buffer.erase(cursor - 1, 1);
                cursor--;
                redraw();
            }
            continue;
        }

        // ================= NORMAL CHAR =================
        buffer.insert(buffer.begin() + cursor, c);
        cursor++;
        redraw();
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
