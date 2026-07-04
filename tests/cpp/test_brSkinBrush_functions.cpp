#include <catch2/catch_test_macros.hpp>

#include "functions.h"

TEST_CASE("lineC produces a Bresenham line between two points", "[brSkinBrush]") {
    std::vector<std::pair<short, short>> posi;
    lineC(0, 0, 3, 0, posi);
    REQUIRE(posi.size() == 4);  // (0,0) (1,0) (2,0) (3,0)
    REQUIRE(posi.front() == std::make_pair((short)0, (short)0));
    REQUIRE(posi.back() == std::make_pair((short)3, (short)0));
}

TEST_CASE("distance_sq of a point to itself is zero", "[brSkinBrush]") {
    point_t a{1.0f, 2.0f, 3.0f};
    REQUIRE(distance_sq(a, a) == 0.0f);
}

TEST_CASE("distance matches the 3-4-5 right triangle", "[brSkinBrush]") {
    point_t a{0.0f, 0.0f, 0.0f};
    point_t b{3.0f, 4.0f, 0.0f};
    REQUIRE(distance(a, b) == 5.0f);
}
