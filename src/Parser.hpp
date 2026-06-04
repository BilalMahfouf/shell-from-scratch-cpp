#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
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

  static std::vector<Token> getTokens(const std::string &str) {
    std::string tokenValue = "";
    std::vector<Token> tokens = {};
    TokenType tokenType;
    Token token;

    bool isSingleQuote = false;
    bool isDoubleQuote = false;

    for (size_t i = 0; i < str.size(); ++i) {
      if (i == (str.size() - 1)) {
        std::cout << "hola";
        token = createToken(tokenValue);
        tokens.push_back(token);
      }

      if (str[i] == ' ' && !isSingleQuote && !isDoubleQuote) {
        token = createToken(tokenValue);
        tokens.push_back(token);
        tokenValue.clear();
        continue;
      }
      if (str[i] == '\'' && !isDoubleQuote) {
        isSingleQuote = !isSingleQuote;
        continue;
      }
      if (str[i] == '\"' && !isSingleQuote) {
        isDoubleQuote = !isDoubleQuote;
        continue;
      }

      tokenValue += str[i];
    }

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
