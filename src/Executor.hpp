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

struct ExecResult {
  enum class Status { Success, Error, Exit };

  std::string output;
  std::string error;
  Status status;
  int exitCode = 0; // added, does not break old code

  static ExecResult Success(std::string out, int code = 0) {
    return {std::move(out), "", Status::Success, code};
  }

  static ExecResult Error(std::string err, int code = 1) {
    return {"", std::move(err), Status::Error, code};
  }

  static ExecResult Exit(int code = -1) { return {"", "", Status::Exit, code}; }

  static ExecResult Empty() { return {"", "", Status::Success, 0}; }
};
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
    std::cout << command << ": not found";
    return std::format("{}: not found", command);
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
          output = std::format("{} is {}", arg, path);
        }
      } else {
        output = std::format("{} is a shell builtin", arg);
      }
      output += '\n';
    }
    return output;
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
  ExecResult runProgram(const std::string &command,
                        std::vector<std::string> args) {

    // std::cout << endl
    //           << "command: " << command << " args: " << args.at(0) << endl;

    const std::string commandPath =
        file_helpers::getExecutableCommandPath(command);

    if (commandPath.empty()) {
      return ExecResult::Error("Invalid command: " + command);
    }

    int stdoutPipe[2];
    int stderrPipe[2];

    if (pipe(stdoutPipe) == -1 || pipe(stderrPipe) == -1) {

      return ExecResult::Error("pipe() failed");
    }

    pid_t pid = fork();

    if (pid == 0) {
      // CHILD

      dup2(stdoutPipe[1], STDOUT_FILENO);
      dup2(stderrPipe[1], STDERR_FILENO);

      close(stdoutPipe[0]);
      close(stdoutPipe[1]);

      close(stderrPipe[0]);
      close(stderrPipe[1]);

      auto argv = getArgsForExecvp(command, args);

      execvp(commandPath.c_str(), argv.data());

      // exec failed
      _exit(127);

    } else if (pid > 0) {

      // PARENT

      close(stdoutPipe[1]);
      close(stderrPipe[1]);

      std::string output;
      std::string error;

      char buffer[4096];
      ssize_t n;

      // read stdout
      while ((n = read(stdoutPipe[0], buffer, sizeof(buffer))) > 0) {

        output.append(buffer, n);
      }

      // read stderr
      while ((n = read(stderrPipe[0], buffer, sizeof(buffer))) > 0) {

        error.append(buffer, n);
      }

      close(stdoutPipe[0]);
      close(stderrPipe[0]);

      int status;
      waitpid(pid, &status, 0);

      if (WIFEXITED(status)) {

        int code = WEXITSTATUS(status);

        if (code == 0) {
          return ExecResult::Success(std::move(output));
        }

        return ExecResult::Error(std::move(error));
      }

      return ExecResult::Exit();

    } else {

      return ExecResult::Error("fork() failed");
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

  ExecResult executeCommandV2(const parser::Command &command) {
    std::string message = "";
    Command commandEnum = getEnumCommand(str::Trim(command.program));

    switch (commandEnum) {
    case Command::Exit:
      return ExecResult::Exit();
    case Command::Echo: {
      if (command.args.size() < 2) {
        return ExecResult::Empty();
      }
      std::vector<string> args(command.args.begin() + 1, command.args.end());
      message = echo(args);
      return ExecResult::Success(message);
    }
    case Command::Type:
      if (command.args.empty())
        return ExecResult::Empty();
      message = type(command.args);
      return ExecResult::Success(message);

    case Command::Pwd: {
      const std::string currentPath =
          file_helpers::getCurrentWorkingDirectory();
      if (!str::isNullOrWhiteSpace(currentPath)) {
        ExecResult::Success(currentPath);
      }
      return ExecResult::Empty();
    }
    case Command::Cd: {

      std::vector<string> args(command.args.begin() + 1, command.args.end());
      if (args.empty())
        // to do make go to home
        return ExecResult::Empty();
      if (args.size() > 1) {
        // std::cout << endl << "-my-shell: cd: too many arguments" << endl;
        message = "-my-shell: cd: too many arguments";
        return ExecResult::Success(message);
      }
      message = args.front();
      cd(str::Trim(message));
      return ExecResult::Empty();
    }
    case Command::None:
      std::vector<string> args(command.args.begin() + 1, command.args.end());
      return runProgram(command.program, args);
    }
    return ExecResult::Empty();
  }

  // to do fix the bug if dir don't exist it don't create it

  void createFileIfDontExist(const std::string file) {
    std::string temp = file;
    if (file.at(0) == '/') {
      temp = "home/bilal" + file;
    }
    fs::path filePath = temp;
    fs::path dir = filePath.parent_path();

    if (!fs::exists(dir) && !dir.empty()) {

      fs::create_directories(dir);
    }
    if (!fs::exists(temp)) {
      std::ofstream(temp).close();
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
      if (output->back() == '\n') {

        std::cout << output.value();
        return;
      }
      std::cout << output.value() << endl;
      return;
    }
    return;
  }

public:
  void run(const parser::ParsedCommand &parsedcommand, bool &exit) {
    for (const auto &command : parsedcommand.commands) {
      if (command.redirections.empty()) {
        auto output = executeCommandV2(command);
        if (output.status == ExecResult::Status::Exit) {
          exit = true;
          return;
        }
        printOutput(output.status == ExecResult::Status::Error ? output.error
                                                               : output.output);
        continue;
      }
      auto redirection = command.redirections.front();
      auto output = executeCommandV2(command);

      if (output.status == ExecResult::Status::Exit) {
        exit = true;
        return;
      }
      if (output.status == ExecResult::Status::Error) {
        continue;
      }
      redirect(redirection, output.output);
    }
  }
  // ["echo","echo bilal is me",[StdOut,"file.txt"],...,..]
};
