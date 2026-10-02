"""Capture README screenshots of the weight editor, paint editor and brush.

This must run inside an interactive (GUI) Maya session, because the viewport
and the Qt windows need a real display. Use the companion launcher, which
starts Maya with a throwaway prefs folder so your own prefs aren't touched:

    tools\\capture_screenshots.bat

Images are written to docs/images/ (or $BLURWEIGHT_SHOT_DIR). Maya quits
when it is done, unless $BLURWEIGHT_SHOT_KEEP_OPEN is set.
"""
import os
import random
import sys
import traceback

from maya import cmds, mel
from maya import OpenMayaUI as omui

from Qt import QtCore, QtGui, QtWidgets, QtCompat

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.environ.get("BLURWEIGHT_SHOT_DIR") or os.path.join(REPO_ROOT, "docs", "images")
LOG_PATH = os.path.join(OUT_DIR, "capture_log.txt")

# Steps run one after another on a timer so Maya and Qt get to repaint in between
STEP_DELAY_MS = 1500
STEPS = []


def log(msg):
    print("[capture] " + msg)
    with open(LOG_PATH, "a") as f:
        f.write(msg + "\n")


def step(func):
    STEPS.append(func)
    return func


def pump(n=20):
    app = QtWidgets.QApplication.instance()
    for _ in range(n):
        app.processEvents()


def mainWindow():
    return QtCompat.wrapInstance(int(omui.MQtUtil.mainWindow()), QtWidgets.QWidget)


def grabWidget(widget, name):
    """Save a widget to disk. Uses a screen grab, so GL content renders properly"""
    widget.raise_()
    widget.activateWindow()
    pump()
    cmds.refresh(force=True)
    pump()
    screen = widget.screen() if hasattr(widget, "screen") else QtWidgets.QApplication.primaryScreen()
    topLeft = widget.mapToGlobal(QtCore.QPoint(0, 0))
    pix = screen.grabWindow(0, topLeft.x(), topLeft.y(), widget.width(), widget.height())
    path = os.path.join(OUT_DIR, name)
    pix.save(path)
    log("saved {} ({}x{})".format(path, pix.width(), pix.height()))


def grabViewport(name):
    """Save the active model panel's viewport"""
    panel = cmds.getPanel(withFocus=True)
    if cmds.getPanel(typeOf=panel) != "modelPanel":
        panel = cmds.getPanel(visiblePanels=True)
        panel = [p for p in panel if cmds.getPanel(typeOf=p) == "modelPanel"][0]
    ptr = omui.MQtUtil.findControl(panel)
    widget = QtCompat.wrapInstance(int(ptr), QtWidgets.QWidget)
    grabWidget(widget, name)


def buildScene():
    """A cylinder bent by a 4 joint chain, with smooth weights"""
    cmds.file(new=True, force=True)
    cmds.select(clear=True)
    joints = [
        cmds.joint(name="joint{}".format(i + 1), position=(0, i * 3, 0)) for i in range(4)
    ]
    cmds.select(clear=True)
    cyl = cmds.polyCylinder(
        name="arm", radius=1.2, height=9, subdivisionsAxis=24, subdivisionsHeight=30
    )[0]
    cmds.move(0, 4.5, 0, cyl)
    cmds.makeIdentity(cyl, apply=True, translate=True)
    skin = cmds.skinCluster(
        joints, cyl, toSelectedBones=True, maximumInfluences=3, dropoffRate=4,
        name="arm_skinCluster",
    )[0]
    cmds.rotate(0, 0, 25, joints[1])
    cmds.rotate(0, 0, 25, joints[2])
    # Distinct joint colors, so the multi-color display has something to show.
    # The weight editor colors columns by the joint's objectColor palette index and the
    # brush uses wireColorRGB, so set both. (This edits the throwaway session's palette)
    colors = [(0.9, 0.3, 0.25), (0.95, 0.75, 0.2), (0.3, 0.75, 0.4), (0.3, 0.5, 0.95)]
    for i, (jnt, col) in enumerate(zip(joints, colors)):
        cmds.displayRGBColor("userDefined{}".format(i + 1), *col)
        cmds.setAttr(jnt + ".useObjectColor", 1)
        cmds.setAttr(jnt + ".objectColor", i)
        cmds.setAttr(jnt + ".wireColorRGB", *col)
    return cyl, joints, skin


def setupViewport(cyl):
    panel = [p for p in cmds.getPanel(visiblePanels=True) if cmds.getPanel(typeOf=p) == "modelPanel"]
    # Single perspective view, shaded, no grid clutter
    mel.eval("setNamedPanelLayout \"Single Perspective View\";")
    panel = cmds.getPanel(withLabel="Persp View")
    cmds.setFocus(panel)
    cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded", grid=False,
                     headsUpDisplay=False, joints=True, wireframeOnShaded=False)
    cmds.viewSet("persp", home=True)
    cmds.select(cyl)
    cmds.viewFit("persp", fitFactor=0.8)
    return panel


STATE = {}


@step
def stepScene():
    # The weight editor loads scripts/.../undoPlug.py, and Maya's Safe Mode blocks plugins
    # from untrusted folders with a modal prompt. Trust this repo for the throwaway session only
    for path in (REPO_ROOT, os.path.join(REPO_ROOT, "scripts")):
        for p in (path, path.replace("\\", "/")):
            cmds.optionVar(stringValueAppend=("SafeModeAllowedlistPaths", p))
    for plugin in ("blurSkin", "brSkinBrush"):
        if not cmds.pluginInfo(plugin, query=True, loaded=True):
            cmds.loadPlugin(plugin)
    cyl, joints, skin = buildScene()
    STATE.update(cyl=cyl, joints=joints, skin=skin)
    win = mainWindow()
    win.showNormal()
    win.resize(1600, 1000)
    win.move(40, 40)
    STATE["panel"] = setupViewport(cyl)
    # Hook the joint colors up to the skinCluster like entering the brush does,
    # so the weight editor shows the same colors
    from mPaintEditor.brushTools.brushPythonFunctions import setColorsOnJoints

    setColorsOnJoints()
    cmds.select(cyl)


@step
def stepWeightEditor():
    from mWeightEditor import runMWeightEditor
    import mWeightEditor

    # A column of vertices up the cylinder, so the rows show a range of weights
    cmds.select(["{}.vtx[{}]".format(STATE["cyl"], i) for i in range(0, 744, 48)])
    runMWeightEditor()
    win = mWeightEditor.WEIGHT_EDITOR
    win.resize(620, 560)
    win.move(900, 120)
    STATE["weightEditor"] = win


@step
def stepWeightEditorShot():
    win = STATE["weightEditor"]
    win.refresh(force=True)
    pump()
    grabWidget(win, "weightEditor.png")
    win.close()


@step
def stepPaintEditor():
    from mPaintEditor import runMPaintEditor
    import mPaintEditor

    cmds.select(STATE["cyl"])
    runMPaintEditor()
    win = mPaintEditor.PAINT_EDITOR
    win.resize(420, 620)
    win.move(1180, 60)
    win.refresh(force=True)
    STATE["paintEditor"] = win


@step
def stepPaintEditorShot():
    win = STATE["paintEditor"]
    # Select an influence in the tree so the shot shows a picked joint
    tree = win.uiInfluenceTREE
    if tree.topLevelItemCount() > 1:
        tree.setCurrentItem(tree.topLevelItem(1))
    pump()
    grabWidget(win, "paintEditor.png")


@step
def stepEnterPaint():
    random.seed(7)  # joint colors are random, keep them stable between runs
    STATE["paintEditor"].enterPaint()


@step
def stepBrushMultiShot():
    # Keep the floating editor window out of the viewport shots
    STATE["paintEditor"].setWindowOpacity(0.0)
    cmds.setFocus(STATE["panel"])
    grabViewport("brushMultiColor.png")


@step
def stepBrushSoloShot():
    win = STATE["paintEditor"]
    win.solo_rb.setChecked(True)
    pump()
    cmds.refresh(force=True)


@step
def stepBrushSoloShot2():
    grabViewport("brushSoloColor.png")


@step
def stepFullWindowShot():
    STATE["paintEditor"].setWindowOpacity(1.0)
    STATE["paintEditor"].multi_rb.setChecked(True)
    pump()
    cmds.refresh(force=True)
    grabWidget(mainWindow(), "paintInContext.png")


@step
def stepDone():
    log("done")
    if not os.environ.get("BLURWEIGHT_SHOT_KEEP_OPEN"):
        cmds.quit(force=True, abort=True)


def runNext():
    if not STEPS:
        return
    func = STEPS.pop(0)
    try:
        log("step: " + func.__name__)
        func()
    except Exception:
        log("FAILED in {}:\n{}".format(func.__name__, traceback.format_exc()))
    QtCore.QTimer.singleShot(STEP_DELAY_MS, runNext)


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    open(LOG_PATH, "w").close()
    scripts = os.path.join(REPO_ROOT, "scripts")
    if scripts not in sys.path:
        sys.path.insert(0, scripts)
    QtCore.QTimer.singleShot(STEP_DELAY_MS, runNext)


main()
