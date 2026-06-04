#pragma once
#include <sstream>
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
  std::vector<Token> getTokens(const std::string &str,
                               const char &separator = ' ') {
    std::stringstream ss(str);
    std::string temp = "";
    std::vector<Token> tokens = {};
    TokenType tokenType;
    Token token;
    while (std::getline(ss, temp, separator)) {
      tokenType = getTokenType(temp);
      token.type = tokenType;
      token.value = temp;
      tokens.push_back(token);
    }
    return tokens;
  }
  inline static const std::unordered_map<std::string, TokenType> tokenTypes{
      {"|", TokenType::PIPE},         {"<", TokenType::REDIRECT_IN},
      {">", TokenType::REDIRECT_OUT}, {">>", TokenType::APPEND},
      {"&&", TokenType::AND},         {"||", TokenType::OR},
      {";", TokenType::SEMICOLON},    {"\n", TokenType::NEWLINE}};

  TokenType getTokenType(const std::string &token) {
    auto it = tokenTypes.find(token);

    if (it != tokenTypes.end())
      return it->second;

    return TokenType::WORD;
  }

public:
  // to do add the logic for the quaotes
  std::vector<Token> ParseInput(const std::string &input) {
    std::vector<Token> tokens = getTokens(input);

    return tokens;
  }
};
