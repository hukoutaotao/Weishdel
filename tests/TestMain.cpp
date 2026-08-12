#include "core/Core.hpp"

#include <cassert>
#include <iostream>

int main()
{
    constexpr int expectedValue = 42;
    const int actualValue = autochess::core::sanityCheckValue();

    assert(actualValue == expectedValue);

    if (actualValue != expectedValue)
    {
        std::cerr
            << "[FAIL] AutoChessCore sanity check: expected "
            << expectedValue
            << ", actual "
            << actualValue
            << '\n';

        return 1;
    }

    std::cout << "[PASS] AutoChessCore sanity check\n";
    return 0;
}