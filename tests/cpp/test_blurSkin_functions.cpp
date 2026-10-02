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

TEST_CASE("CVsAround wraps across the seam when periodic", "[blurSkin]") {
    MIntArray neighbors;
    // corner CV (0,0) on a grid periodic in U picks up the CV at the far end of U
    CVsAround(0, 0, 5, 5, true, false, neighbors);
    REQUIRE(neighbors.length() == 3);
    REQUIRE(getMIntArrayIndex(neighbors, 5 * 4 + 0) != (unsigned int)-1);
}

TEST_CASE("setAverageWeight keeps the current weights when there are no neighbors", "[blurSkin]") {
    MIntArray noNeighbors;
    MIntArray locks(2, 0);
    MDoubleArray full;
    full.append(0.25);
    full.append(0.75);
    MDoubleArray out(2, 0.0);

    setAverageWeight(noNeighbors, 0, 0, 2, locks, full, out);
    REQUIRE(out[0] == 0.25);
    REQUIRE(out[1] == 0.75);
}

TEST_CASE("doPruneWeight leaves an all-zero row at zero instead of NaN", "[blurSkin]") {
    MDoubleArray weights(4, 0.0);
    weights[2] = 0.00001; // below the threshold, gets pruned
    doPruneWeight(weights, 2, 0.0001);
    for (unsigned int i = 0; i < weights.length(); ++i) {
        REQUIRE(weights[i] == 0.0);
    }
}
