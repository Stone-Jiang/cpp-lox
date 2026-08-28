#include "scanner.h"

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
    {"continue", TokenType::CONTINUE},
    {"extend", TokenType::EXTEND},
};

bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') || 
        c == '_' || c == '\'';
}

bool isNumber(char c)
{
    return c >= '0' && c <= '9';
}


Scanner::Scanner(const string& str)
{
    src = str;
    start = src.cbegin();
    cur = src.cbegin();
}

Token Scanner::scan()
{
    skip();
    start = cur;
    tokenLine = line;
    if(isAtEnd())
        return Token(TokenType::TEOF, nullptr, 0, line);
    
    char c = advance();
    if(isAlpha(c))
        return identifier();
    if(isNumber(c))
        return number();

    switch(c)
    {
    case '(': return tok(TokenType::LEFT_PAREN);
    case ')': return tok(TokenType::RIGHT_PAREN);
    case '{': return tok(TokenType::LEFT_BRACE);
    case '}': return tok(TokenType::RIGHT_BRACE);
    case '[': return tok(TokenType::LEFT_SQUARE);
    case ']': return tok(TokenType::RIGHT_SQUARE);
    case ';': return tok(TokenType::SEMICOLON);
    case ':': return tok(TokenType::COLON);
    case ',': return tok(TokenType::COMMA);

    case '-': return tok(TokenType::MINUS);
    case '+': return tok(TokenType::PLUS);
    case '/': return tok(TokenType::SLASH);
    case '*': return tok(TokenType::STAR);
    case '^': return tok(TokenType::CARET);
    case '\\': return tok(TokenType::BACKSLASH);
    case '!':
        return tok(match('=')? TokenType::BANG_EQUAL: TokenType::BANG);
    case '=':
        return tok(match('=')? TokenType::EQUAL_EQUAL: (match('>')? TokenType::RIGHT_ARROW: TokenType::EQUAL));
    case '<':
        return tok(match('=')? TokenType::LESS_EQUAL: TokenType::LESS);
    case '>':
        return tok(match('=')? TokenType::GREATER_EQUAL: TokenType::GREATER);
    case '~':
        return tok(match('=')? TokenType::APPROX_EQUAL: TokenType::TILDE);
    case '.':
        if(match('.') && match('.'))
            return tok(TokenType::ELLIPSIS);
        return tok(TokenType::DOT);
    case '"': return stringy();
    }

    return error(std::format("Unexpected character '{}'.", c));
}

Token Scanner::tok(TokenType type)
{
    return Token(type, &*start, static_cast<int>(distance(start, cur)), tokenLine);
}

Token Scanner::error(const string& msg)
{
    return Token(TokenType::ERROR, msg.c_str(), 0, tokenLine);
}

bool Scanner::isAtEnd()
{
    return cur == src.cend();
}

char Scanner::advance()
{
    return *cur++;
}

char Scanner::peek()
{
    return isAtEnd()? '\0': *cur;
}

char Scanner::peekNext() 
{
    if (isAtEnd() || std::next(cur) == src.cend())
        return '\0';
    return *(cur+1);
}

bool Scanner::match(char exp)
{
    if(isAtEnd())
        return false;
    if(*cur!=exp)
        return false;
    
    cur++;
    return true;
}

void Scanner::skip()
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

TokenType Scanner::identifierType() const
{
    auto text = string(start, cur);
    auto it = keywords.find(text);
    return it!=keywords.end()? it->second: TokenType::IDENTIFIER;
}

Token Scanner::identifier()
{
    while (isAlpha(peek()) || isNumber(peek()))
        advance();
    return tok(identifierType());
}

Token Scanner::number()
{
    while(isNumber(peek()))
        advance();
    if(peek()=='.' && isNumber(peekNext()))
    {
        advance();
        while(isNumber(peek()))
            advance();
    }

    if(peek() == 'i')
    {
        advance();
        return tok(TokenType::IMAGINARY);
    }

    return tok(TokenType::NUMBER);
}

Token Scanner::stringy()
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
