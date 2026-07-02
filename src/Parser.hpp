#pragma once
#include "./helpers/helper.hpp"
#include "str.h"
#include <cstddef>
#include <execution>
#include <iostream>
#include <iterator>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace parser {

enum class TokenType {
  WORD,         // echo, ls, hello, /usr/bin
  PIPE,         // |
  REDIRECT_IN,  //
  REDIRECT_OUT, // >
  APPEND,       // >>
  AND,          // &&
  OR,           // ||
  SEMICOLON,    // ;
  NEWLINE,
  END_OF_FILE
};

struct Token {
  TokenType type;
  std::string value;

  bool operator==(const Token &other) const { return value == other.value; }
};
enum class RedirectionType {
  Stdout,    // >
  Stderr,    // 2>
  AppendOut, // >>
  AppendErr, // 2>>
  Stdin      // <
};
struct Redirection {
  RedirectionType type;
  std::string file;
};
struct Command {
  std::string program;
  std::vector<std::string> args;
  std::vector<Redirection> redirections;
};
struct ParsedCommand {
  std::vector<Command> commands;
};

class Parser {
private:
  static std::string toString(TokenType type) {
    switch (type) {
    case TokenType::WORD:
      return "WORD";
    case TokenType::PIPE:
      return "PIPE";
    case TokenType::REDIRECT_IN:
      return "REDIRECT_IN";
    case TokenType::REDIRECT_OUT:
      return "REDIRECT_OUT";
    case TokenType::APPEND:
      return "APPEND";
    case TokenType::AND:
      return "AND";
    case TokenType::OR:
      return "OR";
    case TokenType::SEMICOLON:
      return "SEMICOLON";
    case TokenType::NEWLINE:
      return "NEWLINE";
    case TokenType::END_OF_FILE:
      return "END_OF_FILE";
    }

    return "UNKNOWN";
  }

  static bool isSpecialShellChar(char c) {
    static const std::unordered_set<char> specials = {
        '$', '\\', '\n', '|', '>', '<', '&', ';', '"', '\''};

    return specials.find(c) != specials.end();
  }
  static bool isCommandControlChar(char c) {
    static const std::unordered_set<char> controls = {'|', '>', '<'};

    return controls.find(c) != controls.end();
  }
  static std::vector<Token> getTokens(const std::string &str) {
    std::string tokenValue = "";
    std::vector<Token> tokens = {};
    TokenType tokenType;
    Token token;
    std::string temp = "";

    bool isSingleQuote = false;
    bool isDoubleQuote = false;
    bool isBackSlash = false;

    for (size_t i = 0; i < str.size(); ++i) {
      // echo multiple\ \ \ \ spaces
      if (isBackSlash && !isSingleQuote) {
        if (isSpecialShellChar(str.at(i))) {
          tokenValue += str.at(i);
        } else {
          tokenValue += str.at(i);
        }
        isBackSlash = false;
        continue;
      }
      if (isCommandControlChar(str.at(i)) && !isSingleQuote && !isDoubleQuote &&
          !isBackSlash) {
        temp = {str.at(i - 1), str.at(i), str.at(i + 1)};
        if (temp == "1>>") {
          tokenValue.pop_back();
          if (!str::isNullOrWhiteSpace(tokenValue)) {
            token = createToken(tokenValue);
            tokens.push_back(token);
            tokenValue.clear();
          }
          token = createToken(temp);
          tokens.push_back(token);
          ++i;
          continue;
        }
        if (temp == "2>>") {
          tokenValue.pop_back();
          if (!str::isNullOrWhiteSpace(tokenValue)) {
            token = createToken(tokenValue);
            tokens.push_back(token);
            tokenValue.clear();
          }
          token = createToken(temp);
          tokens.push_back(token);
          ++i;
          continue;
        }

        temp = {str.at(i - 1), str.at(i)};
        if (temp == "2>") {
          tokenValue.pop_back();
          if (!str::isNullOrWhiteSpace(tokenValue)) {
            token = createToken(tokenValue);
            tokens.push_back(token);
            tokenValue.clear();
          }
          token = createToken(temp);
          tokens.push_back(token);
          continue;
        }
        temp = {str.at(i - 1), str.at(i)};
        if (temp == "1>") {
          tokenValue.pop_back();
          if (!str::isNullOrWhiteSpace(tokenValue)) {
            token = createToken(tokenValue);
            tokens.push_back(token);
            tokenValue.clear();
          }
          token = createToken(temp);
          tokens.push_back(token);
          continue;
        }

        if (!str::isNullOrWhiteSpace(tokenValue)) {
          token = createToken(tokenValue);
          tokens.push_back(token);
          tokenValue.clear();
        }
        if (str.at(i + 1) == str.at(i)) {
          std::string t = {str.at(i + 1), str.at(i)};
          token = createToken(t);
          tokens.push_back(token);
          ++i;
          continue;
        }

        std::string tmp(1, str.at(i));

        token = createToken(tmp);
        tokens.push_back(token);
        continue;
      }
      // echo "bilal is me  n">file.txt

      if (str[i] == ' ' && !isSingleQuote && !isDoubleQuote) {
        if (str::isNullOrWhiteSpace(tokenValue))
          continue;

        token = createToken(tokenValue);
        tokens.push_back(token);
        tokenValue.clear();
        continue;
      }
      if (str.at(i) == '\\' && !isSingleQuote) {
        isBackSlash = !isBackSlash;
        continue;
      }
      if (str[i] == '\'' && !isDoubleQuote) {
        isSingleQuote = !isSingleQuote;
      } else if (str[i] == '\"' && !isSingleQuote) {
        isDoubleQuote = !isDoubleQuote;
      } else {

        tokenValue += str[i];
      }

      // if (i == (str.size() - 1) || i == str.size()) {
      //   token = createToken(tokenValue);
      //   tokens.push_back(token);
      // }
    }
    token = createToken(tokenValue);
    tokens.push_back(token);

    return tokens;
  }
  static Token createToken(const std::string &tokenValue) {
    TokenType tokenType = getTokenType(tokenValue);
    Token token{.type = tokenType, .value = tokenValue};
    return token;
  }
  inline static const std::unordered_map<std::string, TokenType> tokenTypes{
      {"|", TokenType::PIPE},          {"<", TokenType::REDIRECT_IN},
      {">", TokenType::REDIRECT_OUT},  {"1>", TokenType::REDIRECT_OUT},
      {"2>", TokenType::REDIRECT_OUT}, {">>", TokenType::APPEND},
      {"1>>", TokenType::APPEND},      {"2>>", TokenType::APPEND},
      {"&&", TokenType::AND},          {"||", TokenType::OR},
      {";", TokenType::SEMICOLON},     {"\n", TokenType::NEWLINE}};

  static TokenType getTokenType(const std::string &token) {
    auto it = tokenTypes.find(token);

    if (it != tokenTypes.end())
      return it->second;

    return TokenType::WORD;
  }

  /**
   * @brief Tries to convert a token into a RedirectionType.
   *
   * If the token is not a redirection operator, returns std::nullopt.
   *
   * @param token Raw token string (e.g. ">", "2>", ">>", etc.)
   * @return std::optional<RedirectionType> containing the type if valid,
   *         or std::nullopt if it's not a redirection.
   */
  static std::optional<RedirectionType>
  getRedirectionType(const std::string &token) {
    if (token == ">")
      return RedirectionType::Stdout;

    if (token == "1>")
      return RedirectionType::Stdout;

    if (token == "2>")
      return RedirectionType::Stderr;

    if (token == ">>")
      return RedirectionType::AppendOut;

    if (token == "1>>")
      return RedirectionType::AppendOut;

    if (token == "2>>")
      return RedirectionType::AppendErr;

    if (token == "<")
      return RedirectionType::Stdin;

    return std::nullopt;
  }

public:
  std::vector<Token> ParseInput(const std::string &input) {
    std::vector<Token> tokens = getTokens(input);
    return tokens;
  }
  void printTokens(const std::vector<Token> &tokens) {
    for (const auto &token : tokens) {
      std::cout
          << "-------------------------------------------------------------"
          << std::endl;
      std::cout << "Token Type: " << toString(token.type) << std::endl;
      std::cout << "Token: " << token.value << std::endl;
    }
  }
  std::vector<Token> lex(const std::string &input) {
    std::vector<Token> tokens = getTokens(input);
    return tokens;
  }
  std::vector<std::vector<Token>>
  splitByPipe(const std::vector<Token> &tokens) {
    std::vector<std::vector<Token>> result{};
    std::vector<Token> t;
    for (const auto &token : tokens) {
      if (token.type == TokenType::PIPE) {
        result.push_back(t);
        t.clear();
        continue;
      }
      t.push_back(token);
    }
    result.push_back(t);
    return result;
  }
  void deleteTokens(std::vector<Token> &tokens,
                    const std::vector<Token> &tokensToErase) {
    for (const auto &t : tokensToErase) {
      auto it = std::find(tokens.begin(), tokens.end(), t);
      if (it != tokens.end()) {
        tokens.erase(it);
      }
    }
    return;
  }

  // when i wrote this code i wanted to pass the tests so it work but you will
  // need some times to understand it
  void replaceEnvVarsWithTheirData(std::vector<Token> &tokens) {
    std::vector<Token> tokensToErase{};
    std::string temp = "";
    for (auto &token : tokens) {
      if (token.type != TokenType::WORD) {
        continue;
      }
      auto index = token.value.find('$');
      if (index == std::string::npos) {
        continue;
      }
      auto result = helper::getEnvVarValue(token.value.substr(index + 1));
      auto bracesBeginIndex = token.value.find("{");

      if (bracesBeginIndex != std::string::npos) {
        auto bracesEndIndex = token.value.find('}');
        if (bracesEndIndex == std::string::npos) {
          continue;
        }
        // bilal${PATH}bilal
        auto varName = token.value.substr(
            bracesBeginIndex + 1, bracesEndIndex - bracesBeginIndex - 1);

        auto varValue = helper::getEnvVarValue(varName);
        temp = token.value.substr(0, bracesBeginIndex - 1);
        temp += varValue;
        temp += token.value.substr(bracesEndIndex + 1);
        if (str::isNullOrWhiteSpace(temp)) {
          tokensToErase.push_back(token);
          continue;
        }
        token.value = temp;
        continue;
      }

      if (str::isNullOrWhiteSpace(result)) {
        tokensToErase.push_back(token);
        continue;
      }
      if (token.value.starts_with('$')) {

        token.value = result;
      } else {
        temp = token.value.substr(0, index);
        temp += result;
        token.value = temp;
      }
    }
    deleteTokens(tokens, tokensToErase);
    return;
  }
  ParsedCommand parseInput(std::vector<Token> tokens) {
    ParsedCommand parsedCommand;
    std::vector<Command> commands;
    std::vector<Redirection> redirection;
    Command command;
    replaceEnvVarsWithTheirData(tokens);

    auto [args, index] = joinWords(tokens);
    command.args = args;
    command.program = args.at(0);
    commands.push_back(command);
    if (index == tokens.size() - 1) {

      parsedCommand.commands = commands;
      return parsedCommand;
    }
    if (tokens.at(index).type == TokenType::PIPE) {
      commands.clear();
      auto result = splitByPipe(tokens);
      for (const auto &r : result) {
        auto [args, index] = joinWords(r);
        command.args = args;
        command.program = args.at(0);
        commands.push_back(command);
      }
      parsedCommand.commands = commands;
      return parsedCommand;
    }

    auto redirectionType = getRedirectionType(tokens.at(index).value);
    if (redirectionType.has_value()) {
      Redirection redirect{.type = redirectionType.value()};
      if (index + 1 > tokens.size() - 1) {
        return parsedCommand;
      }
      // here means the command still need redirections or extra args
      commands.clear();

      auto token = tokens.at(index + 1);
      redirect.file = token.value;
      command.redirections.push_back(redirect);
      if (index + 2 > tokens.size() - 1) {
        commands.push_back(command);
        parsedCommand.commands = commands;
        return parsedCommand;
      }
      std::vector<Token> restTokens(tokens.begin() + index + 2, tokens.end());
      auto [restArgs, newIndex] = joinWords(restTokens);
      command.args.insert(command.args.end(), restArgs.begin(), restArgs.end());
      commands.push_back(command);
      parsedCommand.commands = commands;
      return parsedCommand;

    } else {
      // here it means no redirect so it's a future me problem for now .
    }

    // ["echo","bilal",">","file.txt"]

    return parsedCommand;
  }
  std::string RedirectionTypeToString(RedirectionType type) {
    switch (type) {
    case RedirectionType::Stdout:
      return "stdout (>)";
    case RedirectionType::Stderr:
      return "stderr (2>)";
    case RedirectionType::AppendOut:
      return "append stdout (>>)";
    case RedirectionType::AppendErr:
      return "append stderr (2>>)";
    case RedirectionType::Stdin:
      return "stdin (<)";
    default:
      return "unknown";
    }
  }
  void printParsedCommand(const ParsedCommand &pc) {
    std::cout << "================ ParsedCommand ================\n";

    for (size_t i = 0; i < pc.commands.size(); ++i) {
      const Command &cmd = pc.commands[i];

      std::cout << "\n[Command " << i << "]\n";

      std::cout << "Program: " << cmd.program << "\n";

      std::cout << "Args: ";
      if (cmd.args.empty()) {
        std::cout << "(none)";
      } else {
        for (const auto &arg : cmd.args)
          std::cout << arg << " ";
      }
      std::cout << "\n";

      std::cout << "Redirections:\n";
      if (cmd.redirections.empty()) {
        std::cout << "  none\n";
      } else {
        for (const auto &r : cmd.redirections) {
          std::cout << "  " << RedirectionTypeToString(r.type) << " -> "
                    << r.file << "\n";
        }
      }
    }

    std::cout << "==============================================\n";
  }

  /**
   * @brief Joins consecutive WORD tokens into a list of arguments.
   *
   * This function iterates through a vector of tokens starting from index 0
   * and collects all consecutive WORD tokens into a string vector.
   *
   * It stops parsing when it encounters the first non-WORD token
   * (such as PIPE, REDIRECT, AND, etc.).
   *
   * @return A tuple containing:
   *         - std::vector<std::string>: collected arguments (WORD tokens)
   *         - size_t: index of the first non-WORD token (where parsing
   * stopped)
   *
   * Example:
   * Input tokens:  echo hello world | ls
   * Output:
   *   args = ["echo", "hello", "world"]
   *   index = position of PIPE token
   */
  static std::tuple<std::vector<std::string>, size_t>
  joinWords(const std::vector<Token> tokens) {
    std::vector<std::string> args{};
    auto index = tokens.size() - 1;

    for (size_t i{0}; i < tokens.size(); ++i) {
      // this to unsure that if there is a pipe or a redirect it will stop
      if (tokens.at(i).type != TokenType::WORD) {
        index = i;
        break;
      }
      args.push_back(tokens.at(i).value);
    }

    return {args, index};
  }
};

// Redirection {type,file} -> Command{program,args,[Redirection]} ->
// ParsedCommand {[Command]}

//  tokens -> command -> PipelineStage -> ParsedCommand
}; // namespace parser
