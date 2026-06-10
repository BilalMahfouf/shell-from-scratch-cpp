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
  // Parser parser;
  // Executor executer;
  // bool exit = false;
  // //
  // // while (true) {
  // //   const std::string input = readUserCommand();
  // //   std::vector<Token> tokens = parser.ParseInput(input);
  // //   // parser.printTokens(tokens);
  // //
  // //   executer.run(tokens, exit);
  // //   if (exit) {
  // //     std::cout << endl << "---------------------------------" << endl;
  // //     std::cout << "good by";
  // //     std::cout << endl << "---------------------------------" << endl;
  // //     break;
  // //   }
  // // }
  // //
  // const std::string input = readUserCommand();
  // auto tokens = parser.lex(input);
  // auto parsedCommand = parser.parseInput(tokens);
  // parser.printParsedCommand(parsedCommand);
  // // parser.printTokens(tokens);
  // //
  // //
  TestRunner t;
  Parser p;

  std::cout << "Running Parser Tests...\n\n";

  parser_tests::test_basic_lexing(t, p);
  parser_tests::test_redirection_detection(t, p);
  parser_tests::test_parser_basic_command(t, p);
  parser_tests::test_parser_redirection(t, p);

  t.summary();
}
