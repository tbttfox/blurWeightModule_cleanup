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

TEST_CASE("DoubleChunks indexes every triangle of every face", "[brSkinBrush]") {
    // face 0 is a quad split into 2 triangles, face 1 is a single triangle
    MIntArray counts, tris;
    counts.append(2);
    counts.append(1);
    for (int v : {0, 1, 2, 0, 2, 3, 4, 5, 6}) {
        tris.append(v);
    }
    DoubleChunks<int> chunks;
    chunks.set(counts, tris);

    REQUIRE(chunks.length() == 2);
    REQUIRE(chunks.length2(0) == 2);
    REQUIRE(chunks.length2(1) == 1);
    auto t1 = chunks(0, 1);
    REQUIRE((t1[0] == 0 && t1[1] == 2 && t1[2] == 3));
    auto t2 = chunks(1, 0);
    REQUIRE((t2[0] == 4 && t2[1] == 5 && t2[2] == 6));
}

TEST_CASE("FlatCounts built from a map includes the largest key", "[brSkinBrush]") {
    std::unordered_map<int, std::vector<int>> rows;
    rows[0] = {1, 2};
    rows[2] = {7};
    FlatCounts<int> fc;
    fc.set(rows);

    REQUIRE(fc.length() == 3);
    REQUIRE(fc[0].size() == 2);
    REQUIRE(fc[1].empty());
    REQUIRE((fc[2].size() == 1 && fc[2][0] == 7));
}

TEST_CASE("findClosestWithinThreshold pairs close, unconnected vertices", "[brSkinBrush]") {
    // 0 and 1 are close but connected. 2 is close to 0 and not connected. 3 is far away
    const float pos[] = {
        0.0f, 0.0f, 0.0f, //
        0.01f, 0.0f, 0.0f, //
        0.0f, 0.02f, 0.0f, //
        5.0f, 0.0f, 0.0f, //
    };
    FlatCounts<int> conn;
    conn.set(std::vector<std::vector<int>>{{1}, {0}, {}, {}});

    // duplicates are expected, border edges list each vertex twice
    std::vector<int> border = {0, 1, 2, 3, 0, 2};
    auto result = findClosestWithinThreshold(border, pos, conn, 0.1f, 4);

    REQUIRE(result[0] == 2);
    REQUIRE(result[2] == 0);
    REQUIRE(result[3] == -1);
}

TEST_CASE("RayIntersectsBBox hits and misses a unit box", "[brSkinBrush]") {
    MPoint mn(-1, -1, -1), mx(1, 1, 1);
    REQUIRE(RayIntersectsBBox(mn, mx, MPoint(0.5, 0.5, -10), MVector(0.001, 0.001, 1)));
    REQUIRE_FALSE(RayIntersectsBBox(mn, mx, MPoint(5, 5, -10), MVector(0.001, 0.001, 1)));
}

TEST_CASE("editArrayMirror doesn't double count a shared influence", "[brSkinBrush]") {
    MIntArray locks(2, 0);
    MDoubleArray full;
    full.append(0.5);
    full.append(0.5);
    std::map<int, std::pair<float, float>> values;
    values[0] = {0.2f, 0.0f};
    MDoubleArray out(2, 0.0);

    // both sides paint influence 0
    editArrayMirror(ModifierCommands::Add, 0, 0, 2, locks, full, values, out);
    REQUIRE(std::abs(out[0] - 0.7) < 1e-6);
    REQUIRE(std::abs(out[1] - 0.3) < 1e-6);
}

TEST_CASE("setAverageWeight keeps the current weights when there are no neighbors", "[brSkinBrush]") {
    MIntArray locks(2, 0);
    MDoubleArray full;
    full.append(0.25);
    full.append(0.75);
    MDoubleArray out(2, 0.0);

    setAverageWeight(std::span<const int>(), 0, 0, 2, locks, full, out, 1.0);
    REQUIRE(out[0] == 0.25);
    REQUIRE(out[1] == 0.75);
}
