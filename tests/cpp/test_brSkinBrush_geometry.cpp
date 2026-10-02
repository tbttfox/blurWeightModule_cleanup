// Geometry and container helpers used by the brSkinBrush paint tool

#include "functions.h"
#include "setOverloads.h"
#include "test_helpers.h"

#include <maya/MMatrix.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>

#include <algorithm>

namespace {

std::vector<int> neighborsOf(const std::vector<int> &flat, const std::vector<int> &index, int v)
{
    std::vector<int> out(flat.begin() + index[v], flat.begin() + index[v + 1]);
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

TEST_CASE("getRawNeighbors finds edge and face neighbors without the vertex itself", "[brSkinBrush][geometry]")
{
    // Two quads sharing the edge 1-4:
    //   0 - 1 - 2
    //   |   |   |
    //   3 - 4 - 5
    MIntArray counts = ints({4, 4});
    MIntArray indices = ints({0, 1, 4, 3, 1, 2, 5, 4});
    std::vector<int> faceFlat, faceIndex, edgeFlat, edgeIndex;
    getRawNeighbors(counts, indices, 6, faceFlat, faceIndex, edgeFlat, edgeIndex);

    REQUIRE(faceIndex.size() == 7);
    REQUIRE(edgeIndex.size() == 7);

    REQUIRE(neighborsOf(edgeFlat, edgeIndex, 0) == std::vector<int>{1, 3});
    REQUIRE(neighborsOf(edgeFlat, edgeIndex, 1) == std::vector<int>{0, 2, 4});
    REQUIRE(neighborsOf(faceFlat, faceIndex, 0) == std::vector<int>{1, 3, 4});
    REQUIRE(neighborsOf(faceFlat, faceIndex, 1) == std::vector<int>{0, 2, 3, 4, 5});
}

TEST_CASE("bboxIntersection returns the entry point of a ray into a box", "[brSkinBrush][geometry]")
{
    MPoint hit;
    bool found = bboxIntersection(
        MPoint(-1, -1, -1), MPoint(1, 1, 1), MMatrix(), MPoint(0, 0, -10), MVector(0, 0, 1), hit
    );
    REQUIRE(found);
    REQUIRE(hit.z == Catch::Approx(-1.0));
    REQUIRE(hit.x == Catch::Approx(0.0).margin(1e-9));
}

TEST_CASE("bboxIntersection respects the box's transform", "[brSkinBrush][geometry]")
{
    // Move the box 5 units along x. A ray down the x axis enters it at x = 4
    MMatrix space;
    space[3][0] = 5.0;
    MPoint hit;
    bool found = bboxIntersection(
        MPoint(-1, -1, -1), MPoint(1, 1, 1), space, MPoint(-10, 0, 0), MVector(1, 0, 0), hit
    );
    REQUIRE(found);
    REQUIRE(hit.x == Catch::Approx(4.0));
}

TEST_CASE("bboxIntersection misses a box beside the ray", "[brSkinBrush][geometry]")
{
    MPoint hit;
    REQUIRE_FALSE(bboxIntersection(
        MPoint(-1, -1, -1), MPoint(1, 1, 1), MMatrix(), MPoint(5, 5, -10), MVector(0, 0, 1), hit
    ));
}

TEST_CASE("brSkinBrush CVsAround finds neighbors and wraps periodic edges", "[brSkinBrush][geometry]")
{
    MIntArray interior;
    CVsAround(2, 2, 5, 5, false, false, interior);
    auto sorted = toVector(interior);
    std::sort(sorted.begin(), sorted.end());
    // (1,2) (3,2) (2,1) (2,3) with index = 5 * u + v
    REQUIRE(sorted == std::vector<int>{7, 11, 13, 17});

    MIntArray corner;
    CVsAround(0, 0, 5, 5, false, true, corner);
    // +u, +v, and -v wraps to v = 4
    REQUIRE(corner.length() == 3);
    REQUIRE(getMIntArrayIndex(corner, 4) != (unsigned int)-1);
}

TEST_CASE("CVsAround doesn't add CVs that are already in the list", "[brSkinBrush][geometry]")
{
    MIntArray verts = ints({7});
    CVsAround(2, 2, 5, 5, false, false, verts);
    REQUIRE(verts.length() == 4);
}

TEST_CASE("lineC steps diagonally and backwards", "[brSkinBrush][geometry]")
{
    std::vector<std::pair<short, short>> diag;
    lineC(0, 0, 3, 3, diag);
    REQUIRE(diag.size() == 4);
    REQUIRE(diag[2] == std::make_pair((short)2, (short)2));

    std::vector<std::pair<short, short>> back;
    lineC(5, 1, 1, 1, back);
    REQUIRE(back.size() == 5);
    REQUIRE(back.back() == std::make_pair((short)1, (short)1));
}

TEST_CASE("FlatChunks splits a flat buffer into fixed size rows", "[brSkinBrush][containers]")
{
    const float pts[] = {0, 1, 2, 3, 4, 5};
    FlatChunks<float, 3> chunks;
    chunks.set(pts, 2);
    REQUIRE(chunks.length() == 2);
    REQUIRE(chunks[1][0] == 3.0f);
    REQUIRE(chunks[1][2] == 5.0f);

    // Setting again replaces the old data instead of appending to it
    chunks.set(std::vector<std::array<float, 3>>{{9, 8, 7}});
    REQUIRE(chunks.length() == 1);
    REQUIRE(chunks[0][0] == 9.0f);
}

TEST_CASE("FlatCounts built from rows keeps empty rows", "[brSkinBrush][containers]")
{
    FlatCounts<int> fc;
    fc.set(std::vector<std::vector<int>>{{1, 2}, {}, {3}});
    REQUIRE(fc.length() == 3);
    REQUIRE(fc[1].empty());
    REQUIRE(fc[2][0] == 3);
}

TEST_CASE("FlatCounts built from an empty map is empty", "[brSkinBrush][containers]")
{
    FlatCounts<int> fc;
    fc.set(std::unordered_map<int, std::vector<int>>{});
    REQUIRE(fc.length() == 0);
}

TEST_CASE("unordered_set operators do set math", "[brSkinBrush][containers]")
{
    std::unordered_set<int> a{1, 2, 3}, b{2, 3, 4};
    REQUIRE((a - b) == std::unordered_set<int>{1});
    REQUIRE((a & b) == std::unordered_set<int>{2, 3});
    REQUIRE((a + b) == std::unordered_set<int>{1, 2, 3, 4});
    REQUIRE((a | b) == (a + b));
}

TEST_CASE("sorted vector operators do set math", "[brSkinBrush][containers]")
{
    std::vector<int> a{1, 2, 3}, b{2, 3, 4};
    REQUIRE((a - b) == std::vector<int>{1});
    REQUIRE((a & b) == std::vector<int>{2, 3});
    REQUIRE((a + b) == std::vector<int>{1, 2, 3, 4});
    REQUIRE((a | b) == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("areDagPathArraysEqual compares lengths of empty arrays", "[brSkinBrush][containers]")
{
    MDagPathArray a, b;
    REQUIRE(areDagPathArraysEqual(a, b));
}
