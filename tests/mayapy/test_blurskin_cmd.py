"""First integration test for the blurSkin plugin's blurSkinCmd.

blurSkinCmd is a plain, flag-driven MPxCommand (no mouse/tool interaction
needed), so it can be exercised directly from a script. It's also undoable,
so we check that cmds.undo() actually restores the prior weights.
"""
from maya import cmds

from helpers.scene_fixtures import build_sphere_with_skincluster


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
