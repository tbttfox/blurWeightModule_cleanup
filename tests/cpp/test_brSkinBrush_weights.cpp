// Weight math used by the brSkinBrush paint tool: editArray, editArrayMirror,
// setAverageWeight, doPruneWeight and getMirrorVertices.
//
// The weight arrays are flat: vertex v, influence j lives at v * nbJoints + j.
// editArray/editArrayMirror write their results into a separate short array, one row per
// painted vertex, in the order of the (sorted) map of painted values.

#include "functions.h"
#include "test_helpers.h"

#include <map>
#include <span>

namespace {

// Run editArray on vertex 0 of a single-vertex weight array
MDoubleArray editOne(
    ModifierCommands command, int influence, const MDoubleArray &weights, double value,
    MIntArray locks = MIntArray()
)
{
    int nbJoints = (int)weights.length();
    if (locks.length() == 0) {
        locks = MIntArray(nbJoints, 0);
    }
    MDoubleArray full(weights);
    std::map<int, double> values{{0, value}};
    MDoubleArray out(nbJoints, -1.0);
    MStatus stat = editArray(command, influence, nbJoints, locks, full, values, out);
    REQUIRE(stat == MS::kSuccess);
    return out;
}

} // namespace

TEST_CASE("editArray Add raises the influence and scales the others down", "[brSkinBrush][weights]")
{
    // 0.5 + 0.2 = 0.7. The rest (0.3 and 0.2) shrink to fill the remaining 0.3
    auto out = editOne(ModifierCommands::Add, 0, doubles({0.5, 0.3, 0.2}), 0.2);
    requireWeights(out, {0.7, 0.18, 0.12});
}

TEST_CASE("editArray Remove lowers the influence and scales the others up", "[brSkinBrush][weights]")
{
    auto out = editOne(ModifierCommands::Remove, 0, doubles({0.5, 0.3, 0.2}), 0.2);
    requireWeights(out, {0.3, 0.42, 0.28});
}

TEST_CASE("editArray Remove can't take weight away when nothing else can receive it", "[brSkinBrush][weights]")
{
    auto out = editOne(ModifierCommands::Remove, 0, doubles({1.0, 0.0, 0.0}), 0.5);
    requireWeights(out, {1.0, 0.0, 0.0});
}

TEST_CASE("editArray Absolute sets the influence to the exact value", "[brSkinBrush][weights]")
{
    auto out = editOne(ModifierCommands::Absolute, 0, doubles({0.5, 0.3, 0.2}), 0.9);
    requireWeights(out, {0.9, 0.06, 0.04});
}

TEST_CASE("editArray AddPercent scales the influence by its own weight", "[brSkinBrush][weights]")
{
    // 0.5 + 0.5 * 0.5 = 0.75
    auto out = editOne(ModifierCommands::AddPercent, 0, doubles({0.5, 0.3, 0.2}), 0.5);
    requireWeights(out, {0.75, 0.15, 0.1});
}

TEST_CASE("editArray Add leaves locked influences alone", "[brSkinBrush][weights]")
{
    // Influence 2 is locked at 0.2, so only 0.8 is shared between influences 0 and 1
    auto out = editOne(ModifierCommands::Add, 0, doubles({0.5, 0.3, 0.2}), 0.2, ints({0, 0, 1}));
    requireWeights(out, {0.7, 0.1, 0.2});
}

TEST_CASE("editArray Add can't exceed the weight left over by locked influences", "[brSkinBrush][weights]")
{
    auto out = editOne(ModifierCommands::Add, 0, doubles({0.5, 0.3, 0.2}), 0.9, ints({0, 0, 1}));
    requireWeights(out, {0.8, 0.0, 0.2});
}

TEST_CASE("editArray Sharpen pushes the largest weight up and keeps the total", "[brSkinBrush][weights]")
{
    // A value of 1 squares every weight: 0.25, 0.09, 0.04, then renormalizes to 1
    auto out = editOne(ModifierCommands::Sharpen, 0, doubles({0.5, 0.3, 0.2}), 1.0);
    requireWeights(out, {0.25 / 0.38, 0.09 / 0.38, 0.04 / 0.38});
    REQUIRE(out[0] > 0.5);
    REQUIRE(rowSum(out, 0, 3) == Catch::Approx(1.0));
}

TEST_CASE("editArray writes one output row per painted vertex, in vertex order", "[brSkinBrush][weights]")
{
    // Two vertices. Paint vertex 1 harder than vertex 0
    MDoubleArray full = doubles({0.5, 0.5, 0.2, 0.8});
    MIntArray locks(2, 0);
    std::map<int, double> values{{1, 0.2}, {0, 0.1}};
    MDoubleArray out(4, -1.0);
    REQUIRE(editArray(ModifierCommands::Add, 0, 2, locks, full, values, out) == MS::kSuccess);
    requireWeights(out, {0.6, 0.4, 0.4, 0.6});
}

TEST_CASE("editArray fails cleanly when the output array is too short", "[brSkinBrush][weights]")
{
    MDoubleArray full = doubles({0.5, 0.5});
    MIntArray locks(2, 0);
    std::map<int, double> values{{0, 0.1}};
    MDoubleArray tooShort(1, 0.0);
    REQUIRE(editArray(ModifierCommands::Add, 0, 2, locks, full, values, tooShort) == MS::kFailure);
}

TEST_CASE("editArray fails cleanly when there are fewer locks than influences", "[brSkinBrush][weights]")
{
    MDoubleArray full = doubles({0.5, 0.5});
    MIntArray locks(1, 0);
    std::map<int, double> values{{0, 0.1}};
    MDoubleArray out(2, 0.0);
    REQUIRE(editArray(ModifierCommands::Add, 0, 2, locks, full, values, out) == MS::kFailure);
}

TEST_CASE("editArrayMirror paints two different influences at once", "[brSkinBrush][weights]")
{
    // Influence 0 gets +0.1, its mirror (influence 1) gets +0.05. Influence 2 gives up the rest
    MIntArray locks(3, 0);
    MDoubleArray full = doubles({0.5, 0.3, 0.2});
    std::map<int, std::pair<float, float>> values{{0, {0.1f, 0.05f}}};
    MDoubleArray out(3, -1.0);
    REQUIRE(
        editArrayMirror(ModifierCommands::Add, 0, 1, 3, locks, full, values, out) == MS::kSuccess
    );
    requireWeights(out, {0.6, 0.35, 0.05});
}

TEST_CASE("setAverageWeight at full strength takes the neighbor average", "[brSkinBrush][weights]")
{
    // Vertex 0 is all influence 0, its neighbors 1 and 2 are all influence 1
    MDoubleArray full = doubles({1.0, 0.0, 0.0, 1.0, 0.0, 1.0});
    MIntArray locks(2, 0);
    std::vector<int> neighbors{1, 2};
    MDoubleArray out(2, -1.0);
    setAverageWeight(std::span<const int>(neighbors), 0, 0, 2, locks, full, out, 1.0);
    requireWeights(out, {0.0, 1.0});
}

TEST_CASE("setAverageWeight at half strength blends toward the average", "[brSkinBrush][weights]")
{
    MDoubleArray full = doubles({1.0, 0.0, 0.0, 1.0, 0.0, 1.0});
    MIntArray locks(2, 0);
    std::vector<int> neighbors{1, 2};
    MDoubleArray out(2, -1.0);
    setAverageWeight(std::span<const int>(neighbors), 0, 0, 2, locks, full, out, 0.5);
    requireWeights(out, {0.5, 0.5});
}

TEST_CASE("setAverageWeight keeps locked influences at their current weight", "[brSkinBrush][weights]")
{
    // Influence 0 is locked at 0.2. The neighbors have a different locked weight,
    // but only the unlocked part of the average is used, scaled into the remaining 0.8
    MDoubleArray full = doubles({0.2, 0.8, 0.0, 0.5, 0.0, 0.5, 0.5, 0.0, 0.5});
    MIntArray locks = ints({1, 0, 0});
    std::vector<int> neighbors{1, 2};
    MDoubleArray out(3, -1.0);
    setAverageWeight(std::span<const int>(neighbors), 0, 0, 3, locks, full, out, 1.0);
    requireWeights(out, {0.2, 0.0, 0.8});
}

TEST_CASE("setAverageWeight writes into the requested output row", "[brSkinBrush][weights]")
{
    MDoubleArray full = doubles({1.0, 0.0, 0.0, 1.0});
    MIntArray locks(2, 0);
    std::vector<int> neighbors{1};
    MDoubleArray out(4, -1.0);
    setAverageWeight(std::span<const int>(neighbors), 0, 1, 2, locks, full, out, 1.0);
    requireWeights(out, {-1.0, -1.0, 0.0, 1.0});
}

TEST_CASE("doPruneWeight zeroes small weights and renormalizes", "[brSkinBrush][weights]")
{
    MDoubleArray weights = doubles({0.995, 0.005, 0.6, 0.4});
    doPruneWeight(weights, 2, 0.01);
    requireWeights(weights, {1.0, 0.0, 0.6, 0.4});
}

TEST_CASE("getMirrorVertices adds the mirrored vertex when merging", "[brSkinBrush][weights]")
{
    MIntArray mirror = ints({1, 0, 2}); // 0 <-> 1, 2 is on the axis
    MIntArray edit = ints({0}), mirrorVerts, both;
    MDoubleArray editW = doubles({0.8}), mirrorW, bothW;
    getMirrorVertices(mirror, edit, mirrorVerts, both, editW, mirrorW, bothW, true);

    REQUIRE(toVector(mirrorVerts) == std::vector<int>{1});
    requireWeights(mirrorW, {0.8});
    REQUIRE(toVector(both) == std::vector<int>{0, 1});
    requireWeights(bothW, {0.8, 0.8});
}

TEST_CASE("getMirrorVertices merging keeps the larger weight of overlapping vertices", "[brSkinBrush][weights]")
{
    // Both 0 and its mirror 1 are painted. Vertex 0 ends up with the larger of the two weights,
    // and only the extra amount (0.9 - 0.5) is applied on the mirror side
    MIntArray mirror = ints({1, 0});
    MIntArray edit = ints({0, 1}), mirrorVerts, both;
    MDoubleArray editW = doubles({0.5, 0.9}), mirrorW, bothW;
    getMirrorVertices(mirror, edit, mirrorVerts, both, editW, mirrorW, bothW, true);

    REQUIRE(toVector(both) == std::vector<int>{0, 1});
    requireWeights(bothW, {0.9, 0.9});
    REQUIRE(toVector(mirrorVerts) == std::vector<int>{0});
    requireWeights(mirrorW, {0.4});
}

TEST_CASE("getMirrorVertices without merging mirrors every vertex one to one", "[brSkinBrush][weights]")
{
    MIntArray mirror = ints({1, 0, 2});
    MIntArray edit = ints({0, 2}), mirrorVerts, both;
    MDoubleArray editW = doubles({0.8, 0.3}), mirrorW, bothW;
    getMirrorVertices(mirror, edit, mirrorVerts, both, editW, mirrorW, bothW, false);

    REQUIRE(toVector(mirrorVerts) == std::vector<int>{1, 2});
    requireWeights(mirrorW, {0.8, 0.3});
    REQUIRE(toVector(both) == std::vector<int>{0, 2, 1});
}
