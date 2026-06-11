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
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace std;

std::string readUserCommand() {
  std::string command = "";
  std::cout << "$ ";
  std::getline(std::cin, command);
  return command;
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
    const std::string input = readUserCommand();
    std::vector<parser::Token> tokens = parser.lex(input);
    parser::ParsedCommand parsedCommand = parser.parseInput(tokens);

    executer.runV2(parsedCommand, exit);
    if (exit) {
      std::cout << endl << "---------------------------------" << endl;
      std::cout << "good by";
      std::cout << endl << "---------------------------------" << endl;
      break;
    }
  }
  //
  // const std::string input = readUserCommand();
  // auto tokens = parser.lex(input);
  // auto parsedCommand = parser.parseInput(tokens);
  // parser.printParsedCommand(parsedCommand);
  // // parser.printTokens(tokens);
  // //
  // //
  // TestRunner t;
  // parser::Parser p;
  //
  // std::cout << "Running Parser Tests...\n\n";
  //
  // parser_tests::test_basic_lexing(t, p);
  // parser_tests::test_redirection_detection(t, p);
  // parser_tests::test_parser_basic_command(t, p);
  // parser_tests::test_parser_redirection(t, p);
  //
  // parser_tests::test_multiple_output_redirections(t, p);
  // parser_tests::test_append_and_overwrite(t, p);
  // parser_tests::test_stdout_stderr_mix(t, p);
  // parser_tests::test_redirect_chain_only(t, p);
  //
  // t.summary();
}
