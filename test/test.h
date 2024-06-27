#ifndef TEST_H
#define TEST_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "gtest/gtest.h"

// https://stackoverflow.com/a/10062016/523079
template <typename T, size_t size>
::testing::AssertionResult ArraysMatch(const T (&expected)[size], const T (&actual)[size]) {
    for (size_t i(0); i < size; ++i) {
        if (expected[i] != actual[i]) {
            return ::testing::AssertionFailure()
                   << "expected[" << i << "] (" << +expected[i] << ") != actual[" << i << "] (" << +actual[i] << ")";
        }
    }

    return ::testing::AssertionSuccess();
}

template <class T1, class T2>
::testing::AssertionResult ArraysMatch(const T1 *expected, const T2 *actual, size_t array_len) {
    for (size_t i(0); i < array_len; ++i) {
        if (expected[i] != actual[i]) {
            return ::testing::AssertionFailure()
                   << "expected[" << i << "] (" << +expected[i] << ") != actual[" << i << "] (" << +actual[i] << ")";
            ;
        }
    }

    return ::testing::AssertionSuccess();
}

#endif  // TEST_H
