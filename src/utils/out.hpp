#pragma once

namespace utils
{
    #if __cplusplus >= 202302L
        
        #include <expected>
        using std::expected;
        using std::unexpected;
        using std::bad_expected_access;

    #else

        #include "expected.hpp"
        using tl::expected;
        using tl::unexpected;
        using tl::bad_expected_access;

    #endif

    #if __cplusplus >= 202002L

        #include <optional>

        using std::optional;
        using std::nullopt;

    #else

        #include "optional.hpp"
        
        using tl::optional;
        using tl::nullopt;


    #endif

} // namespace utils
