#include <catch2/catch_test_macros.hpp>
#include <maya/MIntArray.h>

#include "functions.h"

TEST_CASE("getMIntArrayIndex finds an existing value", "[blurSkin]") {
    MIntArray arr;
    arr.append(10);
    arr.append(20);
    arr.append(30);

    REQUIRE(getMIntArrayIndex(arr, 20) == 1);
    REQUIRE(getMIntArrayIndex(arr, 999) == (unsigned int)-1);
}

TEST_CASE("CVsAround finds the 4 neighbors of an interior CV", "[blurSkin]") {
    MIntArray neighbors;
    // 5x5 non-periodic grid, center CV (2,2) has all 4 neighbors in-bounds
    CVsAround(2, 2, 5, 5, false, false, neighbors);
    REQUIRE(neighbors.length() == 4);
}

TEST_CASE("CVsAround respects grid edges when not periodic", "[blurSkin]") {
    MIntArray neighbors;
    // corner CV (0,0) in a non-periodic grid only has 2 neighbors (+U, +V)
    CVsAround(0, 0, 5, 5, false, false, neighbors);
    REQUIRE(neighbors.length() == 2);
}
