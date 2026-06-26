#pragma once
#include "Parser.hpp"
#include "helpers/file_helpers.hpp"
#include "str.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <poll.h>
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

  static ExecResult Create(std::string output, std::string error, int exitCode,
                           pid_t pid) {
    return {.output = std::move(output),
            .error = std::move(error),
            .exitCode = exitCode};
  }
};
struct Job {
  int id;
  pid_t pid;
  std::string command;
  int outFd;
  int errFd;
  std::string stdoutBuffer;
  std::string stderrBuffer;

  bool operator==(const Job &other) const { return pid == other.pid; }
};
class Executor {
private:
  inline static std::vector<Job> jobs;
  inline static int prevId = 0;
  enum class Command {
    Exit = 0,
    Echo,
    Type,
    Pwd,
    Cd,
    Complete,
    Jobs,
    History,
    None
  };

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
    if (str == "jobs")
      return Command::Jobs;
    if (str == "history")
      return Command::History;
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
    case Command::Jobs:
      return "jobs";
    case Command::History:
      return "history";
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
                                       std::vector<string> &tokens,
                                       std::string input = "") {
    std::vector<char *> argv{};
    argv.push_back(command.data());

    if (tokens.empty()) {
      argv.push_back(nullptr);
      if (!str::isNullOrWhiteSpace(input)) {
        argv.push_back(input.data());
      }
      return argv;
    }

    for (auto &token : tokens) {
      argv.push_back(token.data());
    }
    if (!input.empty()) {
      argv.push_back(input.data());
    }

    argv.push_back(nullptr);
    return argv;
  }

  // Make fd non-blocking
  static void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
      throw std::runtime_error("fcntl F_GETFL failed");

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
      throw std::runtime_error("fcntl F_SETFL failed");
  }

  /*
   * Reads from pipe without blocking shell.
   * Returns whatever is available right now.
   */
  std::string readPipeNonBlocking(int fd) {
    std::string result;

    char buffer[4096];

    while (true) {
      ssize_t n = read(fd, buffer, sizeof(buffer));

      if (n > 0) {
        result.append(buffer, n);
      } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        break; // no more data right now
      } else if (n == 0) {
        break;
      } else {
        throw std::runtime_error("read failed");
      }
    }

    return result;
  } // there is a bug here it return at the end of the output or error a \n (new
  // line)
  ExecResult runProgram(const std::string &command,
                        std::vector<std::string> args,
                        const bool &isBackgroundJob = false,
                        const std::string &input = "") {

    std::optional<std::string> commandPath;
    if (command.starts_with("/") || command.starts_with("./")) {

      commandPath = command;

    } else {
      commandPath = findExecutable(command);

      if (!commandPath.has_value()) {
        return ExecResult::Error(command + ": command not found");
      }
    }

    int stdoutPipe[2];
    int stderrPipe[2];
    int inputPipe[2];

    if (pipe(stdoutPipe) == -1 || pipe(stderrPipe) == -1 ||
        pipe(inputPipe) == -1) {

      return ExecResult::Error("pipe() failed");
    }

    pid_t pid = fork();

    if (pid == 0) {
      // CHILD
      if (!isBackgroundJob) {
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
      }
      if (!str::isNullOrWhiteSpace(input)) {
        dup2(inputPipe[0], STDIN_FILENO);
      }

      close(stdoutPipe[0]);
      close(stdoutPipe[1]);

      close(stderrPipe[0]);
      close(stderrPipe[1]);

      close(inputPipe[0]);
      close(inputPipe[1]);

      auto argv = getArgsForExecvp(command, args);

      execvp(commandPath.value().c_str(), argv.data());

      // exec failed
      _exit(127);

    } else if (pid > 0) {

      // PARENT

      close(stdoutPipe[1]);
      close(stderrPipe[1]);

      if (isBackgroundJob) {
        Job job;
        job.id = jobs.size() + 1;
        job.pid = pid;
        job.command = command + " " + str::JoinString(args, " ");
        job.outFd = stdoutPipe[0];
        job.errFd = stderrPipe[0];
        jobs.push_back(job);
        return ExecResult::Empty();
      }
      if (!str::isNullOrWhiteSpace(input)) {
        write(inputPipe[1], input.c_str(), input.size());
      }
      close(inputPipe[1]);

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
      close(inputPipe[0]);

      int status;
      waitpid(pid, &status, 0);

      if (WIFEXITED(status)) {

        int code = WEXITSTATUS(status);

        return ExecResult::Create(output, error, code, pid);
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
      return ExecResult::Empty();
    }
    if (flag == "-r") {
      auto it = registerdSpecifications.find(args.back());
      if (it != registerdSpecifications.end()) {
        registerdSpecifications.erase(it);
      }
    }
    return ExecResult::Empty();
  }
  ExecResult
  executeCommandV2(const parser::Command &command,
                   const std::vector<parser::Command> &commands = {}) {
    std::string message = "";
    Command commandEnum = getEnumCommand(str::Trim(command.program));
    std::vector<std::string> args(command.args.begin() + 1, command.args.end());
    if (commands.size() > 1) {
      return runPipeline(commands);
    }

    if (command.args.back() == "&") {
      args.pop_back();
      auto result = runProgram(command.program, args, true);
      auto output = "[" + std::to_string(jobs.back().id) + "] " +
                    std::to_string(jobs.back().pid);
      result.output = output;
      return result;
    }
    commandType = commandEnum;
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

    case Command::Jobs:
      return getJobs();
    case Command::None:
      return runProgram(command.program, args);
    }
    return ExecResult::Empty();
  }
  inline static Command commandType;

  bool isJobCompleted(const Job &job) {
    int status;
    pid_t p = waitpid(job.pid, &status, WNOHANG);

    if (p == job.pid) {
      return true;
    }
    return false;
  }
  inline static std::vector<Job> jobsToReap{};
  std::string getFinishedJobTerminalOutput(const Job &job,
                                           const std::string sep) {
    std::string message = "[" + std::to_string(job.id) + "]" + sep +
                          "  Done                 " + job.command;
    return str::Trim(message);
  };
  ExecResult getJobs() {

    auto setUpMessage = [](int id, std::string command, std::string sep) {
      return "[" + std::to_string(id) + "]" + sep +
             "  Running                 " + command + " &";
    };

    cleanJobs();

    if (jobs.empty()) {
      return ExecResult::Empty();
    }
    auto recentJob = jobs.back();
    std::string recent;
    if (isJobCompleted(jobs.back())) {
      recent = getFinishedJobTerminalOutput(jobs.back(), "+");
      jobsToReap.push_back(jobs.back());
    } else {
      recent = setUpMessage(recentJob.id, recentJob.command, "+");
    }

    if (jobs.size() == 1) {
      return ExecResult::Success(recent);
    }

    auto secondMostRecentJob = *(jobs.end() - 2);
    std::string secondMostRecent = "";
    if (isJobCompleted(secondMostRecentJob)) {
      secondMostRecent = getFinishedJobTerminalOutput(secondMostRecentJob, "-");
      jobsToReap.push_back(secondMostRecentJob);
    } else {
      secondMostRecent = setUpMessage(secondMostRecentJob.id,
                                      secondMostRecentJob.command, "-");
    }
    std::vector<std::string> output{};
    output.push_back(recent);
    output.insert(output.begin(), secondMostRecent);
    if (jobs.size() == 2) {
      return ExecResult::Success(str::JoinString(output, "\n"));
    }
    std::string temp;
    std::vector<int> indexOfJobsToReap{};
    for (size_t i{0}; i < jobs.size() - 2; ++i) {
      if (isJobCompleted(jobs.at(i))) {
        temp = getFinishedJobTerminalOutput(jobs.at(i), " ");
        jobsToReap.push_back(jobs.at(i));
      } else {
        temp = setUpMessage(jobs.at(i).id, jobs.at(i).command, " ");
      }

      output.insert(output.begin(), temp);
    }
    return ExecResult::Success(str::JoinString(output, "\n"));
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
    auto clear = [&]() {
      addJobsToReap();
      auto r = getCompletedJobsTerminalOutput();
      if (!r.empty()) {
        if (commandType == Command::Jobs) {
          cleanJobs();
          return;
        }
        auto message = str::JoinString(r, "\n");
        if (message.front() == '\n') {
          return;
        }
        if (output.value().back() == '\n') {
          std::cout << message << std::endl;
        } else {
          std::cout << message << std::endl;
        }
        cleanJobs();
      }
    };

    if (output.has_value()) {
      if (output->back() == '\n') {

        std::cout << output.value();
        clear();
        return;
      }
      std::cout << output.value() << endl;
      clear();
      return;
    }
    return;
  }
  inline static std::unordered_map<std::string, std::string>
      registerdSpecifications{};

  // Returns N-1 pipes for N commands. Each pipe is int[2].
  // pipes[i][0] = read end, pipes[i][1] = write end
  std::vector<std::array<int, 2>> createPipes(size_t commandCount) {
    std::vector<std::array<int, 2>> pipes(commandCount - 1);
    for (auto &p : pipes) {
      if (pipe(p.data()) == -1) {
        // clean up already-opened pipes before throwing
        for (auto &opened : pipes) {
          if (opened[0] != -1) {
            close(opened[0]);
            close(opened[1]);
          }
        }
        throw std::runtime_error("pipe() failed");
      }
    }
    return pipes;
  }
  // i      = index of this command (0-based)
  // total  = total number of commands
  // pipes  = all N-1 pipes
  // returns the child pid, or -1 on failure
  pid_t spawnChild(const parser::Command &cmd, size_t i, size_t total,
                   const std::vector<std::array<int, 2>> &pipes,
                   int outputPipe[2]) {
    pid_t pid = fork();
    if (pid != 0)
      return pid;

    if (i > 0)
      dup2(pipes[i - 1][0], STDIN_FILENO);

    if (i < total - 1)
      dup2(pipes[i][1], STDOUT_FILENO);
    else
      dup2(outputPipe[1], STDOUT_FILENO); // last cmd → parent capture pipe

    for (const auto &p : pipes) {
      close(p[0]);
      close(p[1]);
    }
    close(outputPipe[0]);
    close(outputPipe[1]);
    std::vector<std::string> args(cmd.args.begin() + 1, cmd.args.end());
    auto argv = getArgsForExecvp(cmd.program, args);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  ExecResult runPipeline(const std::vector<parser::Command> &commands) {
    if (commands.empty())
      return ExecResult::Empty();
    if (commands.size() == 1) {
      std::vector<std::string> args(commands.front().args.begin() + 1,
                                    commands.front().args.end());
      return runProgram(commands.at(0).program, args);
    }

    // 1. Create the inter-process pipes
    std::vector<std::array<int, 2>> pipes;
    try {
      pipes = createPipes(commands.size());
    } catch (const std::runtime_error &e) {
      return ExecResult::Error(e.what());
    }

    // 2. Create one extra pipe to capture the last command's stdout
    int outputPipe[2];

    auto cleanPipes = [&]() {
      for (auto &p : pipes) {
        close(p[0]);
        close(p[1]);
      }
    };

    if (pipe(outputPipe) == -1) {
      cleanPipes();
      return ExecResult::Error("pipe() failed for output capture");
    }

    // 3. Spawn all children
    std::vector<pid_t> pids;
    for (size_t i = 0; i < commands.size(); ++i) {
      pid_t pid =
          spawnChild(commands[i], i, commands.size(), pipes, outputPipe);
      if (pid == -1) {
        // fork failed — kill already-spawned children and bail
        for (pid_t p : pids)
          kill(p, SIGTERM);

        cleanPipes();
        close(outputPipe[0]);
        close(outputPipe[1]);
        return ExecResult::Error("fork() failed");
      }
      pids.push_back(pid);
    }

    // 4. Parent closes ALL pipe ends — it doesn't use them
    cleanPipes();
    close(outputPipe[1]); // close write end so we get EOF when last child exits

    // 5. Read last command's output
    std::string output;
    char buffer[4096];
    ssize_t n;
    while ((n = read(outputPipe[0], buffer, sizeof(buffer))) > 0) {
      write(STDOUT_FILENO, buffer, n); // ✓ tester sees output in real time
    }
    close(outputPipe[0]);

    // 6. Wait for ALL children (not just the last one)
    int lastStatus = 0;
    for (size_t i = 0; i < pids.size(); ++i) {
      int status;
      waitpid(pids[i], &status, 0);
      if (i == pids.size() - 1 && WIFEXITED(status)) {
        lastStatus = WEXITSTATUS(status);
      }
    }
    return ExecResult::Empty();
  }

public:
  void run(const parser::ParsedCommand &parsedcommand, bool &exit) {

    std::string input = "";
    auto type = getEnumCommand(parsedcommand.commands.back().program);

    for (size_t i{0}; i < parsedcommand.commands.size(); ++i) {
      const auto &command = parsedcommand.commands[i];
      if (parsedcommand.commands.size() > 1) {
        auto type = getEnumCommand(parsedcommand.commands.back().program);
        if (type != Command::None) {
          auto output = executeCommandV2(parsedcommand.commands.back());
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
          return;
        }
      }

      if (command.redirections.empty()) {

        auto output = executeCommandV2(command, parsedcommand.commands);

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
        break;
        // continue;
      }

      auto redirection = command.redirections.front();
      auto output = executeCommandV2(command);

      if (output.status == ExecResult::Status::Exit) {
        exit = true;
        return;
      }

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

  void setEnviromentVariable(const std::string &name,
                             const std::string &value) {
    auto result = setenv(name.data(), value.data(), 1);
  }
  std::vector<std::string>
  customCompletion(const std::vector<std::string> &args,
                   const std::string &compLine, const size_t &compPoint) {

    if (args.empty()) {
      return {};
    }

    setEnviromentVariable("COMP_LINE", compLine);
    setEnviromentVariable("COMP_POINT", std::to_string(compPoint));

    const auto it = registerdSpecifications.find(args.front());
    if (it == registerdSpecifications.end()) {
      return {};
    }

    std::string script = it->second;

    // Strip optional surrounding quotes (safe + simple)
    if (script.size() >= 2 && script.front() == '\'' && script.back() == '\'') {
      script = script.substr(1, script.size() - 2);
    }

    // args layout:
    // args[0] = command
    // args.back() = current word
    // args[size-2] = previous word (if exists)

    const std::string &current = args.back();

    std::string previous;
    if (args.size() >= 2) {
      previous = args[args.size() - 2];
    }

    const std::vector<std::string> argv = {args.front(), current, previous};

    auto result = runProgram(script, {argv});
    if (result.output.empty()) {
      return {};
    }
    return str::Split(result.output, "\n");
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
  void checkBackgroundJobs() {
    for (auto it = jobs.begin(); it != jobs.end();) {

      int status;
      pid_t result = waitpid(it->pid, &status, WNOHANG);

      // 1. read stdout
      auto out = readPipeNonBlocking(it->outFd);
      it->stdoutBuffer += out;

      // 2. read stderr
      auto err = readPipeNonBlocking(it->errFd);
      it->stderrBuffer += err;

      // 3. process still running
      if (result == 0) {
        ++it;
        continue;
      }

      // 4. process finished
      if (result == it->pid) {

        // final drain (very important)
        while (true) {
          auto o = readPipeNonBlocking(it->outFd);
          auto e = readPipeNonBlocking(it->errFd);

          if (o.empty() && e.empty())
            break;

          it->stdoutBuffer += o;
          it->stderrBuffer += e;
        }

        close(it->outFd);
        close(it->errFd);

        // now job is fully done
        std::cout << std::endl << it->stdoutBuffer << std::endl;
        it = jobs.erase(it);
      } else {
        ++it;
      }
    }
  }
  void cleanJobs() {
    if (jobsToReap.empty()) {
      return;
    }

    for (const auto &j : jobsToReap) {
      auto it = std::find(jobs.begin(), jobs.end(), j);
      if (it != jobs.end()) {
        jobs.erase(it);
      }
    }
    jobsToReap.clear();
    return;
  }
  std::vector<std::string> getCompletedJobsTerminalOutput() {
    if (jobsToReap.empty())
      return {};
    std::vector<std::string> result{};

    result.push_back(getFinishedJobTerminalOutput(jobsToReap.back(), "+"));
    if (jobsToReap.size() == 1) {
      return result;
    }
    result.insert(result.begin(),
                  getFinishedJobTerminalOutput(*(jobsToReap.end() - 2), "-"));
    if (jobsToReap.size() == 2) {
      return result;
    }

    for (size_t i{0}; i < jobsToReap.size() - 2; ++i) {
      result.insert(result.begin(),
                    getFinishedJobTerminalOutput(jobsToReap.at(i), " "));
    }
    return result;
  }
  void addJobsToReap() {
    if (jobs.empty())
      return;

    for (const auto &j : jobs) {
      if (isJobCompleted(j)) {
        jobsToReap.push_back(j);
      }
    }
  }
};
