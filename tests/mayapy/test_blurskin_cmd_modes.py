"""Each blurSkinCmd mode, run against a real skinCluster.

Vertex 0 of a 3 joint sphere starts at weights [0.5, 0.3, 0.2]. The add/absolute/percentage
modes change the listed joints and scale the other unlocked joints to keep the total at 1.
"""
from maya import cmds

from helpers.scene_fixtures import (
    assert_weights,
    build_nurbs_plane_with_skincluster,
    build_sphere_with_skincluster,
    get_weights,
    set_weights,
)

START = [0.5, 0.3, 0.2]


def _scene():
    scene = build_sphere_with_skincluster(num_joints=3)
    scene["vtx0"] = scene["mesh"] + ".vtx[0]"
    set_weights(scene["skinCluster"], scene["vtx0"], scene["joints"], START)
    assert_weights(get_weights(scene["skinCluster"], scene["vtx0"]), START)
    return scene


def _run(scene, command, joint_values, **kwargs):
    joints = [scene["joints"][i] for i, _ in joint_values]
    values = [v for _, v in joint_values]
    cmds.blurSkinCmd(
        command=command,
        meshName=scene["mesh"],
        listVerticesIndices=kwargs.pop("verts", [0]),
        listJoints=joints,
        listJointsValues=values,
        **kwargs
    )
    return get_weights(scene["skinCluster"], scene["vtx0"])


def test_add_raises_the_joint_and_scales_the_others():
    scene = _scene()
    # 0.5 + 0.2. The other joints share the remaining 0.3 in their original ratio
    assert_weights(_run(scene, "add", [(0, 0.2)]), [0.7, 0.18, 0.12])


def test_add_past_one_normalizes_and_zeroes_the_others():
    scene = _scene()
    assert_weights(_run(scene, "add", [(0, 0.8)]), [1.0, 0.0, 0.0])


def test_absolute_sets_the_exact_weight():
    scene = _scene()
    assert_weights(_run(scene, "absolute", [(0, 0.9)]), [0.9, 0.06, 0.04])


def test_percentage_scales_the_current_weight():
    scene = _scene()
    # 0.5 * (1 + 0.5) = 0.75
    assert_weights(_run(scene, "percentage", [(0, 0.5)]), [0.75, 0.15, 0.1])


def test_absolute_on_two_joints():
    scene = _scene()
    assert_weights(_run(scene, "absolute", [(0, 0.4), (1, 0.4)]), [0.4, 0.4, 0.2])


def test_percent_mvt_blends_with_the_original_weights():
    scene = _scene()
    # Halfway between the start and the result of the full add, [0.7, 0.18, 0.12]
    result = _run(scene, "add", [(0, 0.2)], percentMvt=0.5)
    assert_weights(result, [0.6, 0.24, 0.16])


def test_locked_joints_keep_their_weight():
    scene = _scene()
    cmds.setAttr(scene["joints"][2] + ".lockInfluenceWeights", 1)
    # Only the 0.8 on joints 0 and 1 can move
    assert_weights(_run(scene, "add", [(0, 0.2)]), [0.7, 0.1, 0.2])


def test_only_the_listed_vertices_change():
    scene = _scene()
    vtx1 = scene["mesh"] + ".vtx[1]"
    set_weights(scene["skinCluster"], vtx1, scene["joints"], START)
    _run(scene, "add", [(0, 0.2)])
    assert_weights(get_weights(scene["skinCluster"], vtx1), START)


def test_add_is_undoable():
    scene = _scene()
    _run(scene, "add", [(0, 0.2)])
    cmds.undo()
    assert_weights(get_weights(scene["skinCluster"], scene["vtx0"]), START)


def test_average_gives_every_vertex_the_mean_weights():
    scene = _scene()
    skin, joints, mesh = scene["skinCluster"], scene["joints"], scene["mesh"]
    set_weights(skin, mesh + ".vtx[0]", joints, [1.0, 0.0, 0.0])
    set_weights(skin, mesh + ".vtx[1]", joints, [0.0, 1.0, 0.0])

    cmds.blurSkinCmd(command="average", meshName=mesh, listVerticesIndices=[0, 1])
    assert_weights(get_weights(skin, mesh + ".vtx[0]"), [0.5, 0.5, 0.0])
    assert_weights(get_weights(skin, mesh + ".vtx[1]"), [0.5, 0.5, 0.0])


def test_prune_removes_small_weights_on_the_whole_mesh():
    scene = _scene()
    skin, joints, mesh = scene["skinCluster"], scene["joints"], scene["mesh"]
    set_weights(skin, mesh + ".vtx[0]", joints, [0.995, 0.005, 0.0])
    set_weights(skin, mesh + ".vtx[5]", joints, [0.0, 0.004, 0.996])

    cmds.blurSkinCmd(command="prune", meshName=mesh, threshold=0.01)
    assert_weights(get_weights(skin, mesh + ".vtx[0]"), [1.0, 0.0, 0.0])
    assert_weights(get_weights(skin, mesh + ".vtx[5]"), [0.0, 0.0, 1.0])


def test_smooth_depth_reaches_further_neighbors():
    """With a single vertex weighted differently, depth 1 only averages with its direct
    neighbors, which all match, so nothing changes. Depth 2 pulls in a ring that doesn't"""
    scene = build_sphere_with_skincluster(num_joints=2)
    skin, joints, mesh = scene["skinCluster"], scene["joints"], scene["mesh"]
    for i in range(cmds.polyEvaluate(mesh, vertex=True)):
        set_weights(skin, "{}.vtx[{}]".format(mesh, i), joints, [1.0, 0.0])

    # Weight vertex 0's neighbors' neighbors (but not its neighbors) to joint 1
    neighbors = set(cmds.polyInfo(mesh + ".vtx[0]", vertexToEdge=True)[0].split()[2:])
    ring1 = set()
    for edge in neighbors:
        verts = cmds.polyInfo("{}.e[{}]".format(mesh, edge), edgeToVertex=True)[0].split()[2:4]
        ring1.update(int(v) for v in verts)
    ring2 = set()
    for v in ring1:
        for edge in cmds.polyInfo("{}.vtx[{}]".format(mesh, v), vertexToEdge=True)[0].split()[2:]:
            verts = cmds.polyInfo("{}.e[{}]".format(mesh, edge), edgeToVertex=True)[0].split()[2:4]
            ring2.update(int(x) for x in verts)
    for v in ring2 - ring1:
        set_weights(skin, "{}.vtx[{}]".format(mesh, v), joints, [0.0, 1.0])

    cmds.blurSkinCmd(command="smooth", meshName=mesh, listVerticesIndices=[0], depth=1)
    assert_weights(get_weights(skin, mesh + ".vtx[0]"), [1.0, 0.0])

    cmds.blurSkinCmd(command="smooth", meshName=mesh, listVerticesIndices=[0], depth=2)
    assert get_weights(skin, mesh + ".vtx[0]")[1] > 0.0


def test_query_returns_the_weights_of_the_listed_joints():
    scene = _scene()
    result = cmds.blurSkinCmd(
        query=True,
        meshName=scene["mesh"],
        listVerticesIndices=[0],
        listJoints=[scene["joints"][0], scene["joints"][2]],
    )
    assert_weights(result, [0.5, 0.2])


def test_zero_influences_flags_the_joints_used_by_the_vertices():
    scene = _scene()
    skin, joints, mesh = scene["skinCluster"], scene["joints"], scene["mesh"]
    set_weights(skin, mesh + ".vtx[0]", joints, [1.0, 0.0, 0.0])
    set_weights(skin, mesh + ".vtx[1]", joints, [0.0, 0.0, 1.0])

    result = cmds.blurSkinCmd(zeroInfluences=True, meshName=mesh, listVerticesIndices=[0, 1])
    assert result == [1, 0, 1]


def test_selection_is_used_when_no_mesh_is_passed():
    scene = _scene()
    cmds.select(scene["vtx0"])
    cmds.blurSkinCmd(command="add", listJoints=[scene["joints"][0]], listJointsValues=[0.2])
    assert_weights(get_weights(scene["skinCluster"], scene["vtx0"]), [0.7, 0.18, 0.12])


def test_nurbs_add():
    plane = build_nurbs_plane_with_skincluster(num_joints=2)
    skin, joints, shape = plane["skinCluster"], plane["joints"], plane["shape"]
    cv = shape + ".cv[1][3]"
    set_weights(skin, cv, joints, [0.5, 0.5])
    neighbor_before = get_weights(skin, shape + ".cv[1][2]")

    cmds.blurSkinCmd(
        command="add",
        meshName=shape,
        listCVsIndices=[(1, 3)],
        listJoints=[joints[0]],
        listJointsValues=[0.25],
    )
    assert_weights(get_weights(skin, cv), [0.75, 0.25])
    # Its neighbor wasn't listed, so it doesn't change
    assert_weights(get_weights(skin, shape + ".cv[1][2]"), neighbor_before)


def test_help_runs():
    cmds.blurSkinCmd(help=True)
