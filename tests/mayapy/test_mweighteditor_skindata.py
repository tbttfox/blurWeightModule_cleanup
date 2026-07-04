"""First test for mWeightEditor's DataOfSkin data-layer class.

getAllData() is the real entry point: it calls getDataFromSelection()
internally, which reads Maya's *live selection* (cmds.ls(selection=True)),
then populates self.theSkinCluster / self.nbDrivers / etc. So the mesh
transform must be selected before calling getAllData().
"""
from maya import cmds

from helpers.scene_fixtures import build_sphere_with_skincluster
from mWeightEditor.weightTools.skinData import DataOfSkin


def test_dataofskin_loads_real_skincluster():
    scene = build_sphere_with_skincluster()
    cmds.select(scene["transform"], replace=True)

    data = DataOfSkin(createDisplayLocator=False)
    loaded = data.getAllData(displayLocator=False)

    assert loaded is not False
    assert data.theSkinCluster == scene["skinCluster"]
    assert data.nbDrivers == len(scene["joints"])
