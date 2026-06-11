#pragma once

#include "../Parser.hpp"
#include "base_tests.hpp"

namespace parser_tests {

void test_basic_lexing(TestRunner &t, parser::Parser &p) {
  auto tokens = p.lex("echo hello world");

  t.assertEq(tokens.size(), 3, "Lexer: token count");

  t.assertEq(tokens[0].value, "echo", "Lexer: first token");
  t.assertEq(tokens[1].value, "hello", "Lexer: second token");
  t.assertEq(tokens[2].value, "world", "Lexer: third token");
}

void test_redirection_detection(TestRunner &t, parser::Parser &p) {
  auto tokens = p.lex("echo hi > file.txt");

  bool found = false;
  for (const auto &tok : tokens) {
    if (tok.type == parser::TokenType::REDIRECT_OUT)
      found = true;
  }

  t.assertTrue(found, "Lexer: detects REDIRECT_OUT");
}

void test_parser_basic_command(TestRunner &t, parser::Parser &p) {
  auto tokens = p.lex("echo hello world");
  auto parsed = p.parseInput(tokens);

  const auto &cmd = parsed.commands[0];

  t.assertEq(cmd.program, "echo", "Parser: program");
  t.assertTrue(cmd.args.size() >= 2, "Parser: args exist");
}

void test_parser_redirection(TestRunner &t, parser::Parser &p) {
  auto tokens = p.lex("echo hi > file.txt");
  auto parsed = p.parseInput(tokens);

  const auto &cmd = parsed.commands[0];

  t.assertEq(cmd.redirections.size(), 1, "Parser: redirection count");

  if (!cmd.redirections.empty()) {
    t.assertEq(cmd.redirections[0].file, "file.txt",
               "Parser: redirection file");
  }
}
void test_multiple_output_redirections(TestRunner &t, parser::Parser &p) {
  auto tokens = p.lex("echo hi > out1.txt > out2.txt");
  auto parsed = p.parseInput(tokens);

  const auto &cmd = parsed.commands[0];

  t.assertEq(cmd.redirections.size(), 2, "Multi redirect: count");

  if (cmd.redirections.size() == 2) {
    t.assertEq(cmd.redirections[0].file, "out1.txt", "First redirect file");
    t.assertEq(cmd.redirections[1].file, "out2.txt", "Second redirect file");
  }
}
void test_append_and_overwrite(TestRunner &t, parser::Parser &p) {
  auto tokens = p.lex("echo hi > out.txt >> append.txt");
  auto parsed = p.parseInput(tokens);

  const auto &cmd = parsed.commands[0];

  t.assertEq(cmd.redirections.size(), 2, "Append+Overwrite count");

  if (cmd.redirections.size() == 2) {
    t.assertTrue(cmd.redirections[0].type == parser::RedirectionType::Stdout,
                 "First is overwrite (>)");

    t.assertTrue(cmd.redirections[1].type == parser::RedirectionType::AppendOut,
                 "Second is append (>>)");
  }
}
void test_stdout_stderr_mix(TestRunner &t, parser::Parser &p) {
  auto tokens = p.lex("echo hi > out.txt 2> err.txt");
  auto parsed = p.parseInput(tokens);

  const auto &cmd = parsed.commands[0];

  t.assertEq(cmd.redirections.size(), 2, "stdout+stderr count");

  if (cmd.redirections.size() == 2) {
    bool hasOut = false;
    bool hasErr = false;

    for (const auto &r : cmd.redirections) {
      if (r.type == parser::RedirectionType::Stdout)
        hasOut = true;
      if (r.type == parser::RedirectionType::Stderr)
        hasErr = true;
    }

    t.assertTrue(hasOut, "Has stdout redirect");
    t.assertTrue(hasErr, "Has stderr redirect");
  }
}
void test_redirect_chain_only(TestRunner &t, parser::Parser &p) {
  auto tokens = p.lex("echo > a.txt > b.txt > c.txt");
  auto parsed = p.parseInput(tokens);

  const auto &cmd = parsed.commands[0];

  t.assertTrue(cmd.redirections.size() >= 3, "Chain redirects: at least 3");
}

} // namespace parser_tests
