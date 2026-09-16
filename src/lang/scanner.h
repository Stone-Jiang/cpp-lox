#pragma once
#include "commons.h"

enum class TokenType: u8
{
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE, LEFT_SQUARE, RIGHT_SQUARE,
    COMMA, DOT, SEMICOLON, COLON, ELLIPSIS,
    MINUS, PLUS, SLASH, STAR, TILDE, CARET,
    BANG, BANG_EQUAL, EQUAL, EQUAL_EQUAL, APPROX_EQUAL,
    GREATER, GREATER_EQUAL, LESS, LESS_EQUAL,
    IDENTIFIER, STRING, NUMBER, IMAGINARY,
    AND, OR, TRUE, FALSE, NIL,
    FOR, WHILE, IF, ELSE, VAR, RANGE,
    FUN, RETURN, FAIL, PRINT, BACKSLASH, ASSERT,
    CLASS, THIS, SUPER, STATIC,
    BREAK, CONTINUE, EXTEND,
    RIGHT_ARROW,
    REPL_EOF, TEOF, ERROR, 
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
    bool repl = false;
public:
    Scanner(const string& str, bool repl = false);

    Token scan();

private:
    inline Token tok(TokenType type)
    {
        return Token(type, &*start, static_cast<int>(distance(start, cur)), tokenLine);
    }

    inline Token error(const string& msg)
    {
        return Token(TokenType::ERROR, msg.c_str(), 0, tokenLine);
    }

    inline bool isAtEnd()
    {
        
    return cur == src.cend();
    }

    inline char advance()
    {
        return *cur++;
    }

    inline char peek()
    {
        return isAtEnd()? '\0': *cur;
    }

    inline char peekNext()
    {
        if (isAtEnd() || std::next(cur) == src.cend())
            return '\0';
        return *(cur+1);
    }

    inline bool match(char exp)
    {
        if(isAtEnd())
            return false;
        if(*cur!=exp)
            return false;
        
        cur++;
        return true;
    }

    void skip();

    TokenType identifierType() const;
    Token identifier();
    Token number();
    Token stringy();
};
