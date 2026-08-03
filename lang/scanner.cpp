#include "scanner.h"

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

Token Scanner::tok(TokenType type)
{
    return Token(type, &*start, static_cast<int>(distance(start, cur)), tokenLine);
}

Token Scanner::error(const string& msg)
{
    return Token(TokenType::ERROR, nullptr, 0, tokenLine);
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
    while (isalpha(peek()) || isdigit(peek()))
        advance();
    return tok(identifierType());
}

Token Scanner::number()
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
