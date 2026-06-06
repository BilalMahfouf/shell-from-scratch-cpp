#pragma once
#include "str.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
  static std::vector<Token> getTokens(const std::string &str) {
    std::string tokenValue = "";
    std::vector<Token> tokens = {};
    TokenType tokenType;
    Token token;

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
      {"|", TokenType::PIPE},         {"<", TokenType::REDIRECT_IN},
      {">", TokenType::REDIRECT_OUT}, {">>", TokenType::APPEND},
      {"&&", TokenType::AND},         {"||", TokenType::OR},
      {";", TokenType::SEMICOLON},    {"\n", TokenType::NEWLINE}};

  static TokenType getTokenType(const std::string &token) {
    auto it = tokenTypes.find(token);

    if (it != tokenTypes.end())
      return it->second;

    return TokenType::WORD;
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
};
