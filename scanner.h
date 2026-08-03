#pragma once
#include "commons.h"

enum class TokenType
{
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
    COMMA, DOT, MINUS, PLUS,
    SEMICOLON, SLASH, STAR,
    BANG, BANG_EQUAL,
    EQUAL, EQUAL_EQUAL,
    GREATER, GREATER_EQUAL,
    LESS, LESS_EQUAL,
    IDENTIFIER, STRING, NUMBER,
    AND, CLASS, ELSE, FALSE,
    FOR, FUN, IF, NIL, OR,
    PRINT, RETURN, SUPER, THIS,
    TRUE, VAR, WHILE,
    TEOF, ERROR, NONE
};

static constexpr unordered_map<string, TokenType> keywords = 
{
    {"and",    TokenType::AND},
    {"class",  TokenType::CLASS},
    {"else",   TokenType::ELSE},
    {"false",  TokenType::FALSE},
    {"for",    TokenType::FOR},
    {"fun",    TokenType::FUN},
    {"if",     TokenType::IF},
    {"nil",    TokenType::NIL},
    {"or",     TokenType::OR},
    {"print",  TokenType::PRINT},
    {"return", TokenType::RETURN},
    {"super",  TokenType::SUPER},
    {"this",   TokenType::THIS},
    {"true",   TokenType::TRUE},
    {"var",    TokenType::VAR},
    {"while",  TokenType::WHILE}
};

struct Token
{
    TokenType type = TokenType::NONE;
    const char* start = nullptr;
    int len = 0;
    int line = 1;

    Token() = default;
    Token(TokenType type, const char* start, int len, int line): type(type), start(start), len(len), line(line)) {}
};

class Scanner
{
    string src;
    int line = 1;
    int tokenLine = 1;
    string::const_iterator start, cur;
public:
    Scanner(const string& str)
    {
        src = str;
        start = src.cbegin();
        cur = src.cbegin();
    }

    Token scan()
    {
        skip();
        start = cur;
        tokenLine = line;
        if(isAtEnd())
            return Token(TokenType::TEOF, nullptr, 0, line);
        
        char c = advance();
        if(isalpha(c))
            return identifier();
        if(isdigit(c))
            return number();

        switch(c)
        {
        case '(': return tok(TokenType::LEFT_PAREN);
        case ')': return tok(TokenType::RIGHT_PAREN);
        case '{': return tok(TokenType::LEFT_BRACE);
        case '}': return tok(TokenType::RIGHT_BRACE);
        case ';': return tok(TokenType::SEMICOLON);
        case ',': return tok(TokenType::COMMA);
        case '.': return tok(TokenType::DOT);
        case '-': return tok(TokenType::MINUS);
        case '+': return tok(TokenType::PLUS);
        case '/': return tok(TokenType::SLASH);
        case '*': return tok(TokenType::STAR);
        case '!':
            return tok(match('=')? TokenType::BANG_EQUAL: TokenType::BANG);
        case '=':
            return tok(match('=')? TokenType::EQUAL_EQUAL: TokenType::EQUAL);
        case '<':
            return tok(match('=')? TokenType::LESS_EQUAL: TokenType::LESS);
        case '>':
            return tok(match('=')? TokenType::GREATER_EQUAL: TokenType::GREATER);
        case '"': return stringy();
        }

        return error(std::format("Unexpected character '{}'.", c));
    }

private:
    Token tok(TokenType type)
    {
        return Token(type, &*start, static_cast<int>(distance(start, cur)),
                     tokenLine);
    }

    Token error(const string& msg)
    {
        return Token(TokenType::ERROR, nullptr, 0, tokenLine, msg);
    }

    bool isAtEnd()
    {
        return cur == src.cend();
    }

    char advance()
    {
        return *cur++;
    }

    char peek()
    {
        return isAtEnd()? '\0': *cur;
    }

    char peekNext() 
    {
        if (isAtEnd() || std::next(cur) == src.cend())
            return '\0';
        return *(cur+1);
    }

    bool match(char exp)
    {
        if(isAtEnd())
            return false;
        if(*cur!=exp)
            return false;
        
        cur++;
        return true;
    }

    void skip()
    {
        while(true)
        {
            char c = peek();
            switch (c)
            {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                advance();
                line++;
                break;
            case '/':
            {
                if(peekNext()=='/')
                    while(peek()!='\n' && !isAtEnd())
                        advance();
                else
                    return;
                break;
            }
            default:
                return;
            }
        }
    }

    TokenType identifierType() const
    {
        auto text = string(start, cur);
        auto it = keywords.find(text);
        return it!=keywords.end()? it->second: TokenType::IDENTIFIER;
    }

    Token identifier()
    {
        while (isalpha(peek()) || isdigit(peek()))
            advance();
        return tok(identifierType());
    }

    Token number()
    {
        while(isdigit(peek()))
            advance();
        if(peek()=='.' && isdigit(peekNext()))
        {
            advance();
            while(isdigit(peek()))
                advance();
        }
        return tok(TokenType::NUMBER);
    }

    Token stringy()
    {
        while(peek()!='"' && !isAtEnd())
        {
            if(peek()=='\n')
            {
                advance();
                line++;
            }
            else
                advance();
        }
        if(isAtEnd())
            return error("Unterminated string.");
        advance();
        return tok(TokenType::STRING);
    }

};
