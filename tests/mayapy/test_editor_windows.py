"""Build the real Weight Editor and Paint Editor windows, headlessly.

conftest.py makes an offscreen QApplication, so these windows are real QWidgets that are
never shown on screen. Just constructing them catches a lot: Qt5 -> Qt6 API removals broke
the Weight Editor in Maya 2025+ without any other test noticing.
"""
import pytest
from maya import cmds

from Qt import QtWidgets

from helpers.scene_fixtures import (
    assert_weights,
    build_sphere_with_skincluster,
    get_weights,
    set_weights,
)


def _pump():
    QtWidgets.QApplication.instance().processEvents()


@pytest.fixture
def windows():
    """Collect windows opened by a test and close them afterwards"""
    opened = []
    yield opened
    for win in opened:
        win.close()
        win.deleteLater()
    _pump()


def _open_weight_editor(windows):
    from mWeightEditor.weightEditorWidget import SkinWeightWin

    win = SkinWeightWin(parent=None)
    windows.append(win)
    win.show()
    win.refresh(force=True)
    _pump()
    return win


def _open_paint_editor(windows):
    from mPaintEditor.paintEditorWidget import SkinPaintWin

    win = SkinPaintWin(parent=None)
    windows.append(win)
    win.show()
    # Without a UI event loop, deferred refreshes run during __init__, before the tree is
    # cleared, so do one more refresh like the GUI ends up doing
    win.refresh(force=True)
    _pump()
    return win


def test_weight_editor_shows_the_selected_vertices(windows):
    scene = build_sphere_with_skincluster(num_joints=3)
    cmds.select(["{}.vtx[{}]".format(scene["mesh"], i) for i in (0, 5, 9)])

    win = _open_weight_editor(windows)
    model = win._tm
    assert model.rowCount() == 3
    # One column per joint, plus the total column
    assert model.columnCount() == 4
    assert list(win.dataOfDeformer.columnsNames[:3]) == scene["joints"]


def test_weight_editor_values_match_the_skincluster(windows):
    scene = build_sphere_with_skincluster(num_joints=3)
    skin, joints, mesh = scene["skinCluster"], scene["joints"], scene["mesh"]
    set_weights(skin, mesh + ".vtx[0]", joints, [0.5, 0.3, 0.2])
    cmds.select(mesh + ".vtx[0]")

    win = _open_weight_editor(windows)
    model = win._tm
    values = [model.realData(model.index(0, c)) for c in range(3)]
    assert_weights(values, [0.5, 0.3, 0.2])
    assert model.data(model.index(0, 0)) == "50"


def test_weight_editor_editing_a_cell_sets_the_weight(windows):
    scene = build_sphere_with_skincluster(num_joints=3)
    skin, joints, mesh = scene["skinCluster"], scene["joints"], scene["mesh"]
    set_weights(skin, mesh + ".vtx[0]", joints, [0.5, 0.3, 0.2])
    cmds.select(mesh + ".vtx[0]")

    win = _open_weight_editor(windows)
    model = win._tm
    index = model.index(0, 0)
    win._tv.selectionModel().select(index, win._tv.selectionModel().SelectionFlag.Select)
    model.setData(index, 90.0)
    _pump()

    # Typing 90 into the joint1 cell sets it to 0.9, and the others share the rest
    assert_weights(get_weights(skin, mesh + ".vtx[0]"), [0.9, 0.06, 0.04])


def test_paint_editor_lists_the_influences(windows):
    scene = build_sphere_with_skincluster(num_joints=3)
    cmds.select(scene["transform"])

    win = _open_paint_editor(windows)
    tree = win.uiInfluenceTREE
    names = [tree.topLevelItem(i).text(1) for i in range(tree.topLevelItemCount())]
    assert sorted(names) == sorted(scene["joints"])
    assert win.dataOfSkin.theSkinCluster == scene["skinCluster"]


def test_paint_editor_influence_colors_come_from_the_joints(windows):
    scene = build_sphere_with_skincluster(num_joints=2)
    cmds.setAttr(scene["joints"][1] + ".wireColorRGB", 0.0, 1.0, 0.0)
    cmds.select(scene["transform"])

    win = _open_paint_editor(windows)
    item = win._treeDicWidgName[scene["joints"][1]]
    assert item.color() == [0, 255, 0]


def test_paint_editor_with_nothing_selected(windows):
    cmds.select(clear=True)
    win = _open_paint_editor(windows)
    assert win.uiInfluenceTREE.topLevelItemCount() == 0
