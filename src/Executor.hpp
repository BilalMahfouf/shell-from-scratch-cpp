#pragma once
#include "Parser.hpp"
#include "helpers/file_helpers.hpp"
#include "str.h"
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
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

  std::vector<std::string> joinWords(const std::vector<parser::Token> tokens) {
    std::vector<std::string> args{};
    for (const parser::Token &token : tokens) {
      // this to unsure that if there is a pipe or a redirect it will stop
      if (token.type != parser::TokenType::WORD) {
        break;
      }
      args.push_back(token.value);
    }
    return args;
  }

  std::string echo(const std::vector<std::string> &args) {
    std::string newMessage = str::JoinString(args, " ");
    // std::cout << newMessage << endl;
    return newMessage;
  }
  std::string printInvalidCommand(const std::string &command) {
    std::cout << command << ": not found \n";
    return std::format("{}: not found \n", command);
  }
  std::string type(const std::vector<std::string> args) {
    std::string output = "";
    for (const std::string &arg : args) {

      const Command command = getEnumCommand(arg);
      if (command == Command::None) {
        string path = file_helpers::getExecutableCommandPath(arg);
        if (path.empty()) {
          output = printInvalidCommand(arg);
        } else {
          cout << arg << " is " << path << endl;
          output = std::format("{} is {}\n", arg, path);
        }
      } else {
        std::cout << arg << " is a shell builtin" << endl;
        output = std::format("{} is a shell builtin \n", arg);
      }
      output += '\n';
    }
    return output;
  }
  void executeCommand(std::string commandStr,
                      const std::vector<parser::Token> &tokens, bool &exit) {
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
      runProgram(commandStr, args);
      break;
    }
  }
  std::vector<char *> getArgsForExecvp(std::string command,
                                       std::vector<string> &tokens) {
    std::vector<char *> argv{};
    argv.push_back(command.data());

    for (auto &token : tokens) {
      argv.push_back(token.data());
    }

    argv.push_back(nullptr);
    return argv;
  }
  std::string runProgram(const std::string &command,
                         std::vector<std::string> args) {
    const std::string commandPath =
        file_helpers::getExecutableCommandPath(command);

    if (commandPath.empty()) {
      return "Invalid command: " + command;
    }

    int pipefd[2];

    if (pipe(pipefd) == -1) {
      return "pipe() failed";
    }

    pid_t pid = fork();

    if (pid == 0) {
      // CHILD

      dup2(pipefd[1], STDOUT_FILENO); // redirect stdout → pipe
      dup2(pipefd[1], STDERR_FILENO); // optional: capture errors too

      close(pipefd[0]);
      close(pipefd[1]);

      auto argv = getArgsForExecvp(command, args);

      execvp(commandPath.c_str(), argv.data());

      _exit(1); // if exec fails
    } else if (pid > 0) {
      // PARENT

      close(pipefd[1]);

      std::string output;
      char buffer[4096];
      ssize_t n;

      while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, n);
      }

      close(pipefd[0]);
      waitpid(pid, nullptr, 0);

      return output;
    } else {
      return "fork() failed";
    }
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

  std::optional<string> executeCommandV2(const parser::Command &command,
                                         bool &exit) {

    std::string message = "";
    Command commandEnum = getEnumCommand(str::Trim(command.program));
    exit = false;

    switch (commandEnum) {
    case Command::Exit:
      exit = true;
      return nullopt;
    case Command::Echo: {
      if (command.args.empty()) {
        return nullopt;
      }
      std::vector<string> args(command.args.begin() + 1, command.args.end());
      return echo(args);
    }
    case Command::Type:
      if (command.args.empty())
        return nullopt;
      return type(command.args);

    case Command::Pwd: {
      const std::string currentPath =
          file_helpers::getCurrentWorkingDirectory();
      if (!str::isNullOrWhiteSpace(currentPath)) {
        return currentPath + "\n";
      }
      return nullopt;
    }
    case Command::Cd: {

      std::vector<string> args(command.args.begin() + 1, command.args.end());
      if (args.empty())
        return nullopt;
      if (args.size() > 1) {
        // std::cout << endl << "-my-shell: cd: too many arguments" << endl;
        return "\n-my-shell: cd: too many arguments\n";
      }
      message = args.front();
      cd(str::Trim(message));
      return nullopt;
    }
    case Command::None:
      std::vector<string> args(command.args.begin() + 1, command.args.end());
      return runProgram(command.program, args);
    }
    return nullopt;
  }

  void createFileIfDontExist(const std::string file) {
    if (!fs::exists(file)) {
      std::ofstream(file).close();
      return;
    }
    return;
  }

  void redirectStdOut(const std::string file, const std::string content) {
    createFileIfDontExist(file);
    std::ofstream out(file);
    if (!str::isNullOrWhiteSpace(content)) {
      out << content;
    }
    out.close();
    return;
  }
  void redirect(parser::Redirection redirect, const std::string content) {
    switch (redirect.type) {
    case ::parser::RedirectionType::Stdout:
      redirectStdOut(redirect.file, content);
      break;
    }
  }

  void printOutput(std::optional<string> output) {
    if (output.has_value()) {
      std::cout << output.value();
      return;
    }
    return;
  }

public:
  void run(const std::vector<parser::Token> &tokens, bool &exit) {
    if (tokens.at(0).type == parser::TokenType::WORD) {
      std::string command = tokens.at(0).value;
      std::vector<parser::Token> newTokens = tokens;
      newTokens.erase(newTokens.begin());
      executeCommand(command, newTokens, exit);
    }
  }
  void runV2(const parser::ParsedCommand &parsedcommand, bool &exit) {
    std::optional<string> output = "";
    for (const auto &command : parsedcommand.commands) {
      if (command.redirections.empty()) {
        output = executeCommandV2(command, exit);
        printOutput(output);
        continue;
      }
      auto redirection = command.redirections.front();
      auto output = executeCommandV2(command, exit);
      if (output.has_value()) {
        redirect(redirection, output.value());
      }
    }
  }
  // ["echo","echo bilal is me",[StdOut,"file.txt"],...,..]
};
