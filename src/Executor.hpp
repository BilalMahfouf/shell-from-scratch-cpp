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
  void executeCommand(std::string commandStr, const std::vector<Token> &tokens,
                      bool &exit) {
    std::vector<std::string> args;

    std::string message = "";
    Command command = getEnumCommand(commandStr);
    exit = false;

    switch (command) {
    case Command::Exit:
      exit = true;
      return;
    case Command::Echo:
      if (tokens.empty())
        return;
      args = joinWords(tokens);
      echo(args);
      break;
    case Command::Type:
      if (tokens.empty())
        return;
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
    case Command::Cd:
      if (tokens.empty())
        return;
      args = joinWords(tokens);
      if (args.size() > 1) {
        std::cout << endl << "-my-shell: cd: too many arguments" << endl;
      }
      message = args.front();
      cd(str::Trim(message));
      break;
    case Command::None:

      args = joinWords(tokens);
      runProgram(commandStr, str::JoinString(args, " "));
      break;
    }
  }
  void runProgram(const std::string &command, const std::string &args) {
    const std::string commandPath =
        file_helpers::getExecutableCommandPath(command);
    if (commandPath == "") {
      printInvalidCommand(command);
      return;
    }
    std::string newArgs = command + " " + args;
    std::system(newArgs.c_str());
  }
  void cd(const std::string &absolutePath) {
    if (str::isNullOrWhiteSpace(absolutePath))
      return;
    if (absolutePath.at(0) == '/' || absolutePath.at(0) == '.') {
      try {
        fs::current_path(absolutePath);
        return;
      } catch (const fs::filesystem_error &e) {
        std::cout << "cd: " << absolutePath << ": No such file or directory"
                  << endl;
        // std::cerr << "Error: " << e.what() << std::endl;
      }
    } else if (absolutePath.at(0) == '~') {
      const char *homeEnv = std::getenv("HOME");
      if (homeEnv != nullptr) {
        const std::string path = homeEnv + absolutePath.substr(1);
        fs::current_path(path);
      }
    } else {
      std::cerr << "Please provide an absolute path " << endl;
    }
  }

public:
  void run(const std::vector<Token> &tokens, bool &exit) {
    if (tokens.at(0).type == TokenType::WORD) {
      std::string command = tokens.at(0).value;
      std::vector<Token> newTokens = tokens;
      newTokens.erase(newTokens.begin());
      executeCommand(command, newTokens, exit);
    }
  }
};
