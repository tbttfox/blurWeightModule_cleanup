"""The blurSkin plugin's display nodes: pointsDisplay and blurSkinDisplay.

Their drawing can't be checked headlessly, but the pointsDisplay bounding box can.
The weight editor uses pointsDisplay to show the selected vertices, so its bounding box
has to cover them in world space, or Maya culls it when the mesh is off-camera.
"""
from maya import cmds


def _points_display_on_cube(offset):
    transform = cmds.polyCube(width=2, height=2, depth=2)[0]
    shape = cmds.listRelatives(transform, shapes=True)[0]
    cmds.move(offset[0], offset[1], offset[2], transform)

    display_transform = cmds.createNode("transform", name="pointsDisplayTransform")
    display = cmds.createNode("pointsDisplay", parent=display_transform)
    cmds.connectAttr(shape + ".outMesh", display + ".inGeometry")
    cmds.setAttr(display + ".inputComponents", 2, "vtx[0]", "vtx[1]", type="componentList")
    return display_transform, display


def test_points_display_bounding_box_is_in_world_space():
    display_transform, _ = _points_display_on_cube((100, 0, 0))
    bbox = cmds.exactWorldBoundingBox(display_transform)
    assert bbox == [99.0, -1.0, -1.0, 101.0, 1.0, 1.0]


def test_points_display_without_geometry_is_empty():
    display = cmds.createNode("pointsDisplay")
    xmin, _, _, xmax, _, _ = cmds.exactWorldBoundingBox(display)
    assert xmin > xmax  # an empty box


def test_blur_skin_display_node_has_its_paintable_attributes():
    node = cmds.createNode("blurSkinDisplay")
    for attr in ("inMesh", "outMesh", "paintAttr", "command", "influenceIndex", "weightList"):
        assert cmds.attributeQuery(attr, node=node, exists=True), attr
    assert cmds.getAttr(node + ".smoothRepeat") == 3
