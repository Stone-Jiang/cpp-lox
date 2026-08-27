#pragma once
#include "commons.h"

enum class TokenType: u8
{
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE, LEFT_SQUARE, RIGHT_SQUARE,
    COMMA, DOT, SEMICOLON, COLON, 
    MINUS, PLUS, SLASH, STAR, TILDE, CARET,
    BANG, BANG_EQUAL, EQUAL, EQUAL_EQUAL, APPROX_EQUAL,
    GREATER, GREATER_EQUAL, LESS, LESS_EQUAL,
    IDENTIFIER, STRING, NUMBER, IMAGINARY,
    AND, OR, TRUE, FALSE, NIL,
    FOR, WHILE, IF, ELSE, VAR,
    FUN, RETURN, FAIL, PRINT,
    CLASS, THIS, SUPER, STATIC,
    BREAK, CONTINUE,
    TEOF, ERROR
};

static const std::unordered_map<string, TokenType> keywords = 
{
    {"and",    TokenType::AND},
    {"class",  TokenType::CLASS},
    {"else",   TokenType::ELSE},
    {"false",  TokenType::FALSE},
    {"fail",   TokenType::FAIL},
    {"for",    TokenType::FOR},
    {"fun",    TokenType::FUN},
    {"if",     TokenType::IF},
    {"nil",    TokenType::NIL},
    {"or",     TokenType::OR},
    {"print",  TokenType::PRINT},
    {"return", TokenType::RETURN},
    {"super",  TokenType::SUPER},
    {"static", TokenType::STATIC},
    {"this",   TokenType::THIS},
    {"true",   TokenType::TRUE},
    {"var",    TokenType::VAR},
    {"while",  TokenType::WHILE},
    {"break",  TokenType::BREAK},
    {"continue",TokenType::CONTINUE}
};

struct Token
{
    TokenType type = TokenType::ERROR;
    const char* start = nullptr;
    int len = 0;
    int line = 1;

    Token() = default;
    Token(TokenType type, const char* start, int len, int line): type(type), start(start), len(len), line(line) {}
};

class Scanner
{
    string src;
    int line = 1;
    int tokenLine = 1;
    string::const_iterator start, cur;
public:
    Scanner(const string& str);

    Token scan();

private:
    Token tok(TokenType type);
    Token error(const string& msg);

    bool isAtEnd();
    char advance();
    char peek();
    char peekNext();
    bool match(char exp);
    void skip();

    TokenType identifierType() const;
    Token identifier();
    Token number();
    Token stringy();

};
