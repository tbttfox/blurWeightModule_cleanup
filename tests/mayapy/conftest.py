"""
Session-wide setup for all mayapy tests.

Maya standalone must be initialized and the plugins loaded at *import time*
of this file (module level), not inside a fixture -- pytest imports every
test module during collection, before any fixture runs. Several test files
import mWeightEditor/mPaintEditor modules that call cmds.* at their own
import time (e.g. mPaintEditor/__init__.py builds a context-name cache), so
by the time those test modules are collected, maya.standalone must already
be initialized or those imports fail with things like
"module 'maya.cmds' has no attribute 'contextInfo'".

This file:
  1. Starts a headless Maya session (maya.standalone.initialize())
  2. Makes sure Maya can find our freshly-built .mll plugins
  3. Loads blurSkin and brSkinBrush so cmds.blurSkinCmd() / cmds.brSkinBrushCmd() exist
  4. Shuts Maya down cleanly when the whole test run finishes
  5. Resets to a brand-new scene before every individual test
"""
import atexit
import os
import sys

import pytest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPTS_DIR = os.path.join(REPO_ROOT, "scripts")
if SCRIPTS_DIR not in sys.path:
    sys.path.insert(0, SCRIPTS_DIR)

import maya.standalone

maya.standalone.initialize(name="python")
atexit.register(maya.standalone.uninitialize)

from maya import cmds  # noqa: E402  (must come after standalone.initialize)

# The compiled .mll files land here after `meson install` (see quick_compile.bat):
#   <repo_root>/modules/blurWeightModule/win64-2026/plug-ins/
_plugin_dir = os.environ.get("BLURWEIGHT_PLUGIN_DIR") or os.path.join(
    REPO_ROOT, "modules", "blurWeightModule", "win64-2026", "plug-ins"
)
os.environ["MAYA_PLUGIN_PATH"] = _plugin_dir + os.pathsep + os.environ.get("MAYA_PLUGIN_PATH", "")

cmds.loadPlugin(os.path.join(_plugin_dir, "blurSkin.mll"))
cmds.loadPlugin(os.path.join(_plugin_dir, "brSkinBrush.mll"))


@pytest.fixture(autouse=True)
def new_scene():
    """Give every single test function a guaranteed-empty scene."""
    cmds.file(new=True, force=True)
    yield
