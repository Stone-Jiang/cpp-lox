#pragma once
#include <cstdio>
#include <iostream>

namespace utils
{
enum class Color {
    // Foreground
    Black,   Red,     Green,  Yellow,
    Blue,    Magenta, Cyan,   White,
    // Bright foreground
    BrightBlack,   BrightRed,     BrightGreen,  BrightYellow,
    BrightBlue,    BrightMagenta, BrightCyan,   BrightWhite,
    // Background
    BgBlack,   BgRed,     BgGreen,  BgYellow,
    BgBlue,    BgMagenta, BgCyan,   BgWhite,
    // Bright background
    BgBrightBlack,   BgBrightRed,     BgBrightGreen,  BgBrightYellow,
    BgBrightBlue,    BgBrightMagenta, BgBrightCyan,   BgBrightWhite,
    // Styles
    Bold, Dim, Italic, Underline, Blink, Reverse, Strikethrough,
};

consteval const char* ansi_code(Color c) {
    switch (c) {
        // Foreground (30–37)
        case Color::Black:   return "\033[30m";
        case Color::Red:     return "\033[31m";
        case Color::Green:   return "\033[32m";
        case Color::Yellow:  return "\033[33m";
        case Color::Blue:    return "\033[34m";
        case Color::Magenta: return "\033[35m";
        case Color::Cyan:    return "\033[36m";
        case Color::White:   return "\033[37m";
        // Bright foreground (90–97)
        case Color::BrightBlack:   return "\033[90m";
        case Color::BrightRed:     return "\033[91m";
        case Color::BrightGreen:   return "\033[92m";
        case Color::BrightYellow:  return "\033[93m";
        case Color::BrightBlue:    return "\033[94m";
        case Color::BrightMagenta: return "\033[95m";
        case Color::BrightCyan:    return "\033[96m";
        case Color::BrightWhite:   return "\033[97m";
        // Background (40–47)
        case Color::BgBlack:   return "\033[40m";
        case Color::BgRed:     return "\033[41m";
        case Color::BgGreen:   return "\033[42m";
        case Color::BgYellow:  return "\033[43m";
        case Color::BgBlue:    return "\033[44m";
        case Color::BgMagenta: return "\033[45m";
        case Color::BgCyan:    return "\033[46m";
        case Color::BgWhite:   return "\033[47m";
        // Bright background (100–107)
        case Color::BgBrightBlack:   return "\033[100m";
        case Color::BgBrightRed:     return "\033[101m";
        case Color::BgBrightGreen:   return "\033[102m";
        case Color::BgBrightYellow:  return "\033[103m";
        case Color::BgBrightBlue:    return "\033[104m";
        case Color::BgBrightMagenta: return "\033[105m";
        case Color::BgBrightCyan:    return "\033[106m";
        case Color::BgBrightWhite:   return "\033[107m";
        // Styles
        case Color::Bold:          return "\033[1m";
        case Color::Dim:           return "\033[2m";
        case Color::Italic:        return "\033[3m";
        case Color::Underline:     return "\033[4m";
        case Color::Blink:         return "\033[5m";
        case Color::Reverse:       return "\033[7m";
        case Color::Strikethrough: return "\033[9m";
    }
    return "\033[0m";   // unreachable
}

inline constexpr const char* RESET = "\033[0m";

template <Color C>
class TerminalColor
{
public:
    TerminalColor()
    {
        std::cout.flush();
        std::fflush(stdout);
        std::fputs(ansi_code(C), stdout);
    }

    ~TerminalColor()
    {
        std::fputs(RESET, stdout);
        std::fflush(stdout);
    }

    TerminalColor(const TerminalColor&) = delete;
    TerminalColor& operator=(const TerminalColor&) = delete;
    TerminalColor(TerminalColor&&) = delete;
    TerminalColor& operator=(TerminalColor&&) = delete;
};
} // namespace utils
