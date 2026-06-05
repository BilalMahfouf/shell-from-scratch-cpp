#pragma once
#include "Parser.hpp"
#include "helpers/file_helpers.hpp"
#include "str.h"
#include <iostream>
#include <vector>

class Executor {
private:
  enum class Command { Exit = 0, Echo, Type, Pwd, Cd, None };

  Command getEnumCommand(const std::string &str) {
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
    case Command::None:
      return "";
    }
    return "";
  }

  std::vector<std::string> joinWords(const std::vector<Token> tokens) {
    std::vector<std::string> args{};
    for (const Token &token : tokens) {
      // this to unsure that if there is a pipe or a redirect it will stop
      if (token.type != TokenType::WORD) {
        break;
      }
      args.push_back(token.value);
    }
    return args;
  }

  void echo(const std::vector<std::string> &args) {
    std::string newMessage = str::JoinString(args, " ");
    std::cout << newMessage << endl;
  }
  void printInvalidCommand(const std::string &command) {
    std::cout << command << ": not found \n";
  }
  void type(const std::vector<std::string> args) {
    for (const std::string &arg : args) {

      const Command command = getEnumCommand(arg);
      if (command == Command::None) {
        string path = file_helpers::getExecutableCommandPath(arg);
        if (path.empty()) {
          printInvalidCommand(arg);
        } else {
          cout << arg << " is " << path << endl;
        }
      } else {
        std::cout << arg << " is a shell builtin" << endl;
      }
    }
  }
  void executeCommand(Command command, const std::vector<Token> &tokens,
                      bool &exit) {
    std::vector<std::string> args = {};
    std::string message = "";
    exit = false;

    switch (command) {
    case Command::Exit:
      std::cout << "\n exit:" << exit;
      exit = true;
      return;
    case Command::Echo:
      args = joinWords(tokens);
      echo(args);
      break;
    case Command::Type:
      args = joinWords(tokens);
      type(args);
      break;

    case Command::Pwd: {
      const std::string currentPath =
          file_helpers::getCurrentWorkingDirectory();
      if (!str::isNullOrWhiteSpace(currentPath)) {
        std::cout << currentPath << endl;
      }
      break;
    }
      // case Command::Cd:
      //   message = input.substr(getStringCommand(Command::Cd).size() + 1);
      //   cd(str::Trim(message));
      //   break;
      // case Command::None:
      //   runProgram(input);
      //   break;
      // }
    }
  }

public:
  void run(const std::vector<Token> &tokens, bool &exit) {
    if (tokens.at(0).type == TokenType::WORD) {
      std::cout << "value: " << tokens.at(0).value << std::endl;
      Command command = getEnumCommand(tokens.at(0).value);
      std::vector<Token> newTokens = tokens;
      newTokens.erase(newTokens.begin());
      executeCommand(command, newTokens, exit);
    }
  }
};
