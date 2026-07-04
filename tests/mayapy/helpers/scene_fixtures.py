"""Reusable scene-building helpers shared across the mayapy test suite."""
from maya import cmds


def build_sphere_with_skincluster(num_joints=2, sphere_name="testSphere"):
    """Create a poly sphere, `num_joints` joints in a simple chain, and a
    skinCluster binding the sphere to all of them.

    Returns
    -------
    dict with keys: "transform", "mesh" (shape node name), "joints" (list),
    "skinCluster".
    """
    cmds.select(clear=True)
    joints = [
        cmds.joint(name="testJoint{}".format(i), position=(0, i * 2, 0))
        for i in range(num_joints)
    ]
    cmds.select(clear=True)

    transform, _ = cmds.polySphere(
        name=sphere_name, radius=2, subdivisionsAxis=8, subdivisionsHeight=8
    )
    mesh_shape = cmds.listRelatives(transform, shapes=True)[0]

    cmds.select(joints, replace=True)
    cmds.select(transform, add=True)
    skin_cluster = cmds.skinCluster(
        joints, transform, toSelectedBones=True, name="testSkinCluster"
    )[0]

    # blurSkinCmd (and the real mWeightEditor UI, via
    # abstractData.addLockVerticesAttribute) expect this attribute to exist
    # on the deformed shape -- without it, getListLockVertices in the C++
    # plugin leaves its output array empty and indexing into it crashes.
    if not cmds.attributeQuery("lockedVertices", node=mesh_shape, exists=True):
        cmds.addAttr(mesh_shape, longName="lockedVertices", dataType="Int32Array")

    return {
        "transform": transform,
        "mesh": mesh_shape,
        "joints": joints,
        "skinCluster": skin_cluster,
    }
