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
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
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

  static ExecResult Create(std::string output, std::string error,
                           int exitCode) {
    return {.output = std::move(output),
            .error = std::move(error),
            .exitCode = exitCode};
  }
};
class Executor {
private:
  enum class Command { Exit = 0, Echo, Type, Pwd, Cd, Complete, None };

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
    if (str == "complete")
      return Command::Complete;

    return Command::None;
  }

  std::optional<std::string> findExecutable(const std::string &command) {
    const char *pathEnv = std::getenv("PATH");

    if (pathEnv == nullptr)
      return {};

    std::stringstream paths(pathEnv);
    std::string directory;

    while (std::getline(paths, directory, ':')) {
      fs::path executablePath = fs::path(directory) / command;

      if (fs::exists(executablePath) &&
          access(executablePath.c_str(), X_OK) == 0) {
        return executablePath.string();
      }
    }

    return {};
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
    case Command::Complete:
      return "complete";
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
    return std::format("{}: not found", command);
  }

  std::string type(std::vector<std::string> args) {
    std::string output = "";
    args.erase(args.begin());

    for (const std::string &arg : args) {
      const Command command = getEnumCommand(arg);

      if (command != Command::None) {
        output += std::format("{} is a shell builtin\n", arg);
        continue;
      }

      auto path = findExecutable(arg);

      if (!path.has_value()) {
        output += printInvalidCommand(arg);
      } else {
        output += std::format("{} is {}\n", arg, path.value());
      }
    }

    return output;
  }

  std::vector<char *> getArgsForExecvp(std::string command,
                                       std::vector<string> &tokens) {
    std::vector<char *> argv{};
    argv.push_back(command.data());

    if (tokens.empty()) {
      argv.push_back(nullptr);
      return argv;
    }

    for (auto &token : tokens) {
      argv.push_back(token.data());
    }

    argv.push_back(nullptr);
    return argv;
  }

  // there is a bug here it return at the end of the output or error a \n (new
  // line)
  ExecResult runProgram(const std::string &command,
                        std::vector<std::string> args) {

    // std::cout << endl
    //           << "command: " << command << " args: " << args.at(0) << endl;

    std::optional<std::string> commandPath;
    if (command.starts_with("/") || command.starts_with("./")) {
      std::cout << "hola";

      commandPath = command;

    } else {
      commandPath = findExecutable(command);

      if (!commandPath.has_value()) {
        return ExecResult::Error(command + ": command not found");
      }
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

      execvp(commandPath.value().c_str(), argv.data());

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

        return ExecResult::Create(output, error, code);
      }

      return ExecResult::Exit();

    } else {

      return ExecResult::Error("fork() failed");
    }
  }

  void cd(const std::string &path) {
    if (str::isNullOrWhiteSpace(path))
      return;

    try {
      if (path.at(0) == '~') {
        const char *homeEnv = std::getenv("HOME");

        if (homeEnv != nullptr) {
          fs::current_path(std::string(homeEnv) + path.substr(1));
          return;
        }
      }

      // handles:
      // cd bilal
      // cd ..
      // cd .
      // cd /home/user
      fs::current_path(path);

    } catch (const fs::filesystem_error &e) {
      std::cerr << "cd: " << path << ": No such file or directory" << std::endl;
    }
  }

  ExecResult complete(const std::vector<std::string> &args) {
    if (args.empty()) {
      return ExecResult::Empty();
    }
    auto flag = args.front();
    if (flag == "-p") {
      auto it = registerdSpecifications.find(args.back());
      if (it != registerdSpecifications.end()) {
        auto outputMessage = "complete -C " + it->second + " " + args.back();
        return ExecResult::Success(outputMessage);
      }
      auto err =
          "complete: " + *(args.begin() + 1) + ": no completion specification";
      return ExecResult::Error(err);
    }
    if (flag == "-C") {
      auto value = "\'" + *(args.begin() + 1) + "\'";
      registerdSpecifications.insert({args.back(), value});
    }
    return ExecResult::Empty();
  }
  ExecResult executeCommandV2(const parser::Command &command) {
    std::string message = "";
    Command commandEnum = getEnumCommand(str::Trim(command.program));
    std::vector<std::string> args(command.args.begin() + 1, command.args.end());

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
        return ExecResult::Success(currentPath);
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
    case Command::Complete:
      return complete(args);
    case Command::None:
      return runProgram(command.program, args);
    }
    return ExecResult::Empty();
  }

  // to do fix the bug if dir don't exist it don't create it

  void createFileIfDontExist(const std::string file) {
    fs::path temp = file;
    if (!fs::exists(temp)) {
      std::ofstream(temp).close();
      return;
    }
    return;
  }

  void redirectStd(const std::string file, const std::string content) {
    createFileIfDontExist(file);
    std::ofstream out(file);
    if (!str::isNullOrWhiteSpace(content)) {
      out << content;
    }
    out.close();
    return;
  }
  void redirectAppend(const std::string file, const std::string content) {
    createFileIfDontExist(file);
    {
      std::string temp = content;
      std::ofstream out(file, std::ios::app);

      bool firstWrite = file_helpers::isEmpty(file);

      if (!str::isNullOrWhiteSpace(content)) {
        if (!firstWrite) {
          out << '\n';
        }
        if (content.back() == '\n') {
          temp = content.substr(0, content.size() - 1);
        }
        out << temp;
      }
    }
    return;
  }
  void redirect(parser::Redirection redirect, const std::string content) {
    switch (redirect.type) {
    case ::parser::RedirectionType::Stdout:
      redirectStd(redirect.file, content);
      break;

    case ::parser::RedirectionType::Stderr:
      redirectStd(redirect.file, content);
      break;
    case ::parser::RedirectionType::AppendErr:
      redirectAppend(redirect.file, content);
      break;

    case ::parser::RedirectionType::AppendOut:
      redirectAppend(redirect.file, content);
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
  inline static std::unordered_map<std::string, std::string>
      registerdSpecifications{};

public:
  void run(const parser::ParsedCommand &parsedcommand, bool &exit) {
    for (const auto &command : parsedcommand.commands) {
      if (command.redirections.empty()) {
        auto output = executeCommandV2(command);
        if (output.status == ExecResult::Status::Exit) {
          exit = true;
          return;
        }
        if (!str::isNullOrWhiteSpace(output.output)) {
          printOutput(output.output);
        }
        if (!str::isNullOrWhiteSpace(output.error)) {
          printOutput(output.error);
        }
        continue;
      }
      auto redirection = command.redirections.front();
      auto output = executeCommandV2(command);

      if (output.status == ExecResult::Status::Exit) {
        exit = true;
        return;
      }
      // std::cout << endl
      //           << "output: " << output.output << endl
      //           << "error: " << output.error << endl;
      if (redirection.type == parser::RedirectionType::Stdout) {
        if (!str::isNullOrWhiteSpace(output.error)) {
          printOutput(output.error);
        }
        redirect(redirection, output.output);
        continue;
      }
      if (redirection.type == parser::RedirectionType::Stderr) {
        if (!str::isNullOrWhiteSpace(output.output)) {
          printOutput(output.output);
        }
        redirect(redirection, output.error);
        continue;
      }
      if (redirection.type == parser::RedirectionType::AppendErr) {
        if (!str::isNullOrWhiteSpace(output.output)) {
          printOutput(output.output);
        }
        redirect(redirection, output.error);
        continue;
      }
      if (redirection.type == parser::RedirectionType::AppendOut) {
        if (!str::isNullOrWhiteSpace(output.error)) {
          printOutput(output.error);
        }
        redirect(redirection, output.output);
        continue;
      }
    }
    // ["echo","echo bilal is me",[StdOut,"file.txt"],...,..]
  }
  std::optional<std::string> customCompletion(const std::string &program) {

    auto it = registerdSpecifications.find(program);

    if (it == registerdSpecifications.end()) {
      return std::nullopt;
    }
    it->second.pop_back();
    it->second.erase(it->second.begin());
    auto result = runProgram(it->second, {it->second});

    if (result.output.back() == '\n') {
      result.output.pop_back();
    }
    return result.output.append(" ");
  }
};
