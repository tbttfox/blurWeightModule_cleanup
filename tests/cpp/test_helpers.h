#pragma once
// Small helpers for building Maya arrays and comparing weights in the tests

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <maya/MDoubleArray.h>
#include <maya/MIntArray.h>

#include <initializer_list>
#include <vector>

inline MDoubleArray doubles(std::initializer_list<double> vals)
{
    MDoubleArray arr;
    for (double v : vals) {
        arr.append(v);
    }
    return arr;
}

inline MIntArray ints(std::initializer_list<int> vals)
{
    MIntArray arr;
    for (int v : vals) {
        arr.append(v);
    }
    return arr;
}

inline std::vector<int> toVector(const MIntArray &arr)
{
    std::vector<int> out;
    for (unsigned int i = 0; i < arr.length(); ++i) {
        out.push_back(arr[i]);
    }
    return out;
}

// Check a contiguous run of `expected.size()` values in `actual`, starting at `offset`
inline void requireWeights(
    const MDoubleArray &actual, std::initializer_list<double> expected, unsigned int offset = 0
)
{
    REQUIRE(actual.length() >= offset + expected.size());
    unsigned int i = offset;
    for (double e : expected) {
        INFO("index " << i);
        REQUIRE(actual[i] == Catch::Approx(e).margin(1e-6));
        ++i;
    }
}

inline double rowSum(const MDoubleArray &arr, unsigned int row, unsigned int nbJoints)
{
    double sum = 0.0;
    for (unsigned int j = 0; j < nbJoints; ++j) {
        sum += arr[row * nbJoints + j];
    }
    return sum;
}
