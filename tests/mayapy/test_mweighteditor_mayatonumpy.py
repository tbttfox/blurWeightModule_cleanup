"""Smoke test for mWeightEditor's mayaToNumpy / numpyToMaya round-trip.

Needs no scene at all -- it's a good first thing to run to confirm the
mayapy harness and sys.path wiring for scripts/ work end-to-end.
"""
from maya import OpenMaya as om

from mWeightEditor.weightTools.mayaToNumpy import mayaToNumpy, numpyToMaya


def test_mayato_numpy_roundtrip():
    src = om.MDoubleArray()
    for v in (1.0, 2.5, 3.75, -4.0):
        src.append(v)

    as_numpy = mayaToNumpy(src)
    assert list(as_numpy) == [1.0, 2.5, 3.75, -4.0]

    back = numpyToMaya(as_numpy, om.MDoubleArray)
    assert [back[i] for i in range(back.length())] == [1.0, 2.5, 3.75, -4.0]
