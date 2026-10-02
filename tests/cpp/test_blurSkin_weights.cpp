// Weight math used by the blurSkinDisplay paint node: editArray, setAverageWeight,
// getMirrorVertices. Commands are ints here:
// 0 Add - 1 Remove - 2 AddPercent - 3 Absolute - 4 Smooth - 5 Sharpen

#include "functions.h"
#include "test_helpers.h"

namespace {

MDoubleArray editOne(int command, int influence, const MDoubleArray &weights, double value)
{
    int nbJoints = (int)weights.length();
    MIntArray locks(nbJoints, 0);
    MDoubleArray full(weights);
    MIntArray verts = ints({0});
    MDoubleArray vertWeights = doubles({value});
    MDoubleArray out(nbJoints, -1.0);
    REQUIRE(
        editArray(command, influence, nbJoints, locks, full, verts, vertWeights, out) ==
        MS::kSuccess
    );
    return out;
}

} // namespace

TEST_CASE("blurSkin editArray Add raises the influence and scales the others down", "[blurSkin][weights]")
{
    auto out = editOne(0, 0, doubles({0.5, 0.3, 0.2}), 0.2);
    requireWeights(out, {0.7, 0.18, 0.12});
}

TEST_CASE("blurSkin editArray Remove scales the others up", "[blurSkin][weights]")
{
    auto out = editOne(1, 0, doubles({0.5, 0.3, 0.2}), 0.2);
    requireWeights(out, {0.3, 0.42, 0.28});
}

TEST_CASE("blurSkin editArray Remove leaves a fully weighted vertex alone", "[blurSkin][weights]")
{
    auto out = editOne(1, 0, doubles({1.0, 0.0}), 0.5);
    requireWeights(out, {1.0, 0.0});
}

TEST_CASE("blurSkin editArray Absolute sets the exact value", "[blurSkin][weights]")
{
    auto out = editOne(3, 0, doubles({0.5, 0.3, 0.2}), 0.9);
    requireWeights(out, {0.9, 0.06, 0.04});
}

TEST_CASE("blurSkin editArray AddPercent scales by the current weight", "[blurSkin][weights]")
{
    auto out = editOne(2, 0, doubles({0.5, 0.3, 0.2}), 0.5);
    requireWeights(out, {0.75, 0.15, 0.1});
}

TEST_CASE("blurSkin editArray Sharpen drops small weights and renormalizes", "[blurSkin][weights]")
{
    // Each weight becomes w * 2 - 2/3, clamped to 0..1, then normalized
    auto out = editOne(5, 0, doubles({0.5, 0.3, 0.2}), 1.0);
    requireWeights(out, {1.0, 0.0, 0.0});
}

TEST_CASE("blurSkin setAverageWeight averages the neighbors", "[blurSkin][weights]")
{
    MDoubleArray full = doubles({1.0, 0.0, 0.0, 1.0, 0.5, 0.5});
    MIntArray locks(2, 0);
    MIntArray neighbors = ints({1, 2});
    MDoubleArray out(2, -1.0);
    setAverageWeight(neighbors, 0, 0, 2, locks, full, out);
    requireWeights(out, {0.25, 0.75});
}

TEST_CASE("blurSkin getMirrorVertices adds the mirrored vertex when merging", "[blurSkin][weights]")
{
    MIntArray mirror = ints({1, 0, 2});
    MIntArray edit = ints({0}), mirrorVerts, both;
    MDoubleArray editW = doubles({0.8}), mirrorW, bothW;
    getMirrorVertices(mirror, edit, mirrorVerts, both, editW, mirrorW, bothW, true);

    REQUIRE(toVector(mirrorVerts) == std::vector<int>{1});
    REQUIRE(toVector(both) == std::vector<int>{0, 1});
    requireWeights(bothW, {0.8, 0.8});
}

TEST_CASE("blurSkin doPruneWeight zeroes small weights and renormalizes", "[blurSkin][weights]")
{
    MDoubleArray weights = doubles({0.995, 0.005, 0.6, 0.4});
    doPruneWeight(weights, 2, 0.01);
    requireWeights(weights, {1.0, 0.0, 0.6, 0.4});
}
