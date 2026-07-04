"""First test for mPaintEditor's cmdSkinCluster.getThreeIndices.

This function is pure integer index math (lattice s/t/u <-> flat index
conversion) and needs no Maya scene at all -- but cmdSkinCluster.py still
does `from maya import cmds, OpenMaya as om, OpenMayaAnim as oma` at module
level, so it must be run under mayapy, not plain python.exe.

Note: getThreeIndices divides with plain `/`, which is true division under
mayapy's Python 3 interpreter, so t/u come back as floats even though they're
conceptually integer indices -- this test reflects that real behavior rather
than assuming clean ints.
"""
from mPaintEditor.brushTools.cmdSkinCluster import getThreeIndices


def test_get_three_indices_pure_math():
    div_s, div_t, div_u = 4, 3, 2
    simple_index = 7  # s=3, t=1, u=0

    s, t, u = getThreeIndices(div_s, div_t, div_u, simple_index)
    assert (s, int(t), int(u)) == (3, 1, 0)

    round_tripped = getThreeIndices(div_s, div_t, div_u, s, t, u)
    assert round_tripped == simple_index
