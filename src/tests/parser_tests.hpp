#pragma once

#include "../Parser.hpp"
#include "base_tests.hpp"

namespace parser_tests {

void test_basic_lexing(TestRunner &t, Parser &p) {
  auto tokens = p.lex("echo hello world");

  t.assertEq(tokens.size(), 3, "Lexer: token count");

  t.assertEq(tokens[0].value, "echo", "Lexer: first token");
  t.assertEq(tokens[1].value, "hello", "Lexer: second token");
  t.assertEq(tokens[2].value, "world", "Lexer: third token");
}

void test_redirection_detection(TestRunner &t, Parser &p) {
  auto tokens = p.lex("echo hi > file.txt");

  bool found = false;
  for (const auto &tok : tokens) {
    if (tok.type == TokenType::REDIRECT_OUT)
      found = true;
  }

  t.assertTrue(found, "Lexer: detects REDIRECT_OUT");
}

void test_parser_basic_command(TestRunner &t, Parser &p) {
  auto tokens = p.lex("echo hello world");
  auto parsed = p.parseInput(tokens);

  const auto &cmd = parsed.commands[0];

  t.assertEq(cmd.program, "echo", "Parser: program");
  t.assertTrue(cmd.args.size() >= 2, "Parser: args exist");
}

void test_parser_redirection(TestRunner &t, Parser &p) {
  auto tokens = p.lex("echo hi > file.txt");
  auto parsed = p.parseInput(tokens);

  const auto &cmd = parsed.commands[0];

  t.assertEq(cmd.redirections.size(), 1, "Parser: redirection count");

  if (!cmd.redirections.empty()) {
    t.assertEq(cmd.redirections[0].file, "file.txt",
               "Parser: redirection file");
  }
}

} // namespace parser_tests
