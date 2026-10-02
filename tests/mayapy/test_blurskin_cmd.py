"""First integration test for the blurSkin plugin's blurSkinCmd.

blurSkinCmd is a plain, flag-driven MPxCommand (no mouse/tool interaction
needed), so it can be exercised directly from a script. It's also undoable,
so we check that cmds.undo() actually restores the prior weights.
"""
from maya import cmds

from helpers.scene_fixtures import (
    build_nurbs_plane_with_skincluster,
    build_sphere_with_skincluster,
)


def test_blurskin_smooth_is_undoable():
    scene = build_sphere_with_skincluster()

    # Push all weight onto joint 0 for vertex 0, so there is something for
    # smoothing to actually change.
    cmds.skinPercent(
        scene["skinCluster"],
        scene["mesh"] + ".vtx[0]",
        transformValue=[(scene["joints"][0], 1.0), (scene["joints"][1], 0.0)],
    )
    before = cmds.skinPercent(
        scene["skinCluster"], scene["mesh"] + ".vtx[0]", query=True, value=True
    )

    cmds.blurSkinCmd(
        command="smooth",
        meshName=scene["mesh"],
        listVerticesIndices=[0],
        repeat=1,
    )
    after = cmds.skinPercent(
        scene["skinCluster"], scene["mesh"] + ".vtx[0]", query=True, value=True
    )
    assert after != before

    cmds.undo()
    restored = cmds.skinPercent(
        scene["skinCluster"], scene["mesh"] + ".vtx[0]", query=True, value=True
    )
    assert restored == before


def test_blurskin_smooth_redo_reapplies():
    scene = build_sphere_with_skincluster()
    cmds.skinPercent(
        scene["skinCluster"],
        scene["mesh"] + ".vtx[0]",
        transformValue=[(scene["joints"][0], 1.0), (scene["joints"][1], 0.0)],
    )
    cmds.blurSkinCmd(
        command="smooth", meshName=scene["mesh"], listVerticesIndices=[0], repeat=1
    )
    after = cmds.skinPercent(
        scene["skinCluster"], scene["mesh"] + ".vtx[0]", query=True, value=True
    )
    cmds.undo()
    cmds.redo()
    redone = cmds.skinPercent(
        scene["skinCluster"], scene["mesh"] + ".vtx[0]", query=True, value=True
    )
    assert redone == after


def test_blurskin_nurbs_smooth_undo_redo():
    plane = build_nurbs_plane_with_skincluster()
    shape, joints, skin_cluster = plane["shape"], plane["joints"], plane["skinCluster"]
    cv = "{}.cv[2][2]".format(shape)
    cmds.skinPercent(skin_cluster, cv, transformValue=[(joints[0], 1.0), (joints[1], 0.0)])
    before = cmds.skinPercent(skin_cluster, cv, query=True, value=True)

    cmds.blurSkinCmd(command="smooth", meshName=shape, listCVsIndices=[(2, 2)], repeat=1)
    after = cmds.skinPercent(skin_cluster, cv, query=True, value=True)
    assert after != before
    assert abs(sum(after) - 1.0) < 1e-6

    cmds.undo()
    assert cmds.skinPercent(skin_cluster, cv, query=True, value=True) == before
    cmds.redo()
    assert cmds.skinPercent(skin_cluster, cv, query=True, value=True) == after
