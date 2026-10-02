# Blur Weight Module

A Maya module with two tools for editing skinCluster weights on meshes and NURBS surfaces:

- **Paint Editor** (`mPaintEditor` + the `brSkinBrush` plugin): A fast skin weight paint brush.
- **Weight Editor** (`mWeightEditor` + the `blurSkin` plugin): A spreadsheet view of the
  weights of the selected vertices, with tools to normalize, prune, smooth and average them.

![The paint editor next to a skinned mesh in Maya](docs/images/paintInContext.png)


## Easy Installation
1. Download [this file](https://raw.githubusercontent.com/blurstudio/blurWeightModule/master/blur_weight_module_installer.py) to your computer. Make sure it's saved as a python file.
2. Drag/drop the python file into a freshly opened instance of Maya (make sure all other mayas are closed). A command prompt window might open for a couple seconds. This is normal.
3. If you have multiple Maya versions installed, repeat step 2 for those versions as well. This just ensures that numpy and Qt.py are installed for those versions.
4. Create python shelf buttons with these scripts
```python
from mWeightEditor import runMWeightEditor
runMWeightEditor()
```

```python
from mPaintEditor import runMPaintEditor
runMPaintEditor()
```

## Manual Installation
1. Download the `blurWeightModule-v*.*.*.zip` file from the [latest release](https://github.com/blurstudio/blurWeightModule/releases/latest)
2. Create a `modules` folder in your maya user directory. For example, on Windows, that would mean creating `C:\Users\<your-username>\Documents\maya\modules`
3. Copy the `blurWeightModule.mod` file and the `blurWeightModule` folder into that directory.
4. Install numpy and Qt.py to mayapy [using pip](https://knowledge.autodesk.com/support/maya/learn-explore/caas/CloudHelp/cloudhelp/2022/ENU/Maya-Scripting/files/GUID-72A245EC-CDB4-46AB-BEE0-4BBBF9791627-htm.html). For example, on Windows, once you're in the right place the command will be `mayapy -m pip install numpy Qt.py`. If you want to keep installs contained to your user prefs, I suggest pointing to the user scripts site-packages folder like this: `mayapy -m pip install --target C:\Users\<your-username>\Documents\maya\<maya-version>\scripts\site-packages`
4. Create python shelf buttons from the scripts just like in the Easy Installation

## Paint Editor

Select a skinned mesh or NURBS surface, then run mPaintEditor

<img src="docs/images/paintEditor.png" alt="The paint editor window" align="right" width="320">

The window lists the influences of the selected skinCluster. To paint:

1. Pick an influence in the list.
2. Press **Paint** to enter the brush, and paint with the left mouse button in the viewport.
3. Change modes at the top: **Add**, **Rmv**, **Add %**, **Abs**, **Smooth**, **Sharpen**,
   **Lock** or **Unlock** (the last two lock and unlock vertices).
4. Set the **Intensity** and **Brush Size** (or use the preset buttons above the sliders). See Also: Brush Hotkeys below

Other controls:

- **Profile**: The falloff of the brush (none, linear, smooth, narrow).
- **Flood**: Applies the current mode to the whole mesh.
- **Post Set**: Waits until you release the mouse to set the weights, which is faster on dense meshes.
- **180 coverage**: Also paints vertices facing away from the camera.
- **Ignore Dfm Locks**: Paints through locked influences.
- **Mirror Active**: Mirrors your strokes across the axis chosen in the options panel.
- **Pick Vert** / **Pick Jnt**: pick the influence to paint from the viewport.
- **Options**: Opens extra settings, including solo color range, draw mode,
  smooth repeats, whether Ctrl or Shift smooths, and mirror tolerance.

<br clear="right">

While painting, the mesh shows the weights. **Multi Color** shows every influence in its own
color. **Solo** shows only the current influence, from black (no weight) to white (full weight):

| Multi Color | Solo |
| --- | --- |
| ![Multi color display](docs/images/brushMultiColor.png) | ![Solo color display](docs/images/brushSoloColor.png) |

The influence colors come from each joint's wireframe color. Use the color swatch in the
influence list to change one.

### Brush hotkeys

These are the defaults. The window's hotkey list always shows the current ones.

| Action | Key |
| --- | --- |
| Paint with the current mode | LMB |
| Remove | Ctrl + LMB |
| Smooth | Shift + LMB |
| Sharpen | Ctrl + Shift + LMB |
| Brush size / strength | MMB drag left-right / up-down (hold Ctrl for fine strength) |
| Marking menu with paint options | U |
| Pick an influence from the joints in the viewport | D |
| Pick the highest weighted influence under the mouse | Alt + D |
| Toggle mirror / solo / solo opaque | Alt + M / Alt + S / Alt + A |
| Toggle the brush wireframe / X-ray joints | Alt + W / Alt + X |
| Set the camera orbit point under the mouse | F |
| Undo the last stroke | Ctrl + Z |
| Exit the brush | Esc |

To swap Ctrl and Shift, so that Ctrl smooths and Shift removes (the XSI layout), choose
**CTRL Smooths** in the options panel.

## Weight Editor

Select a skinned mesh, or some of its vertices, then run the mWeightEditor

![The weight editor showing the weights of a column of vertices](docs/images/weightEditor.png)

Each row is a selected vertex and each column is an influence, with the influence color
under its name. The editor follows your selection. The lock button at the top left pins the
current list of vertices, so it stops following the selection.

- **Editing values:** Select cells, then type a value or drag the slider. **Abs**, **Add**
  and **Add %** choose how the slider value is applied. The preset buttons above the slider
  set common values.
- **Weight tools:** **Normalize** makes each vertex add up to 100. **Prune** removes weights
  below the threshold. **Smooth** averages each vertex with its neighbors. **Average** gives
  every selected vertex the same weights.
- **Locks:** Right-click a column header to lock or unlock influences, or a row header to
  lock or unlock vertices. Edits leave locked weights alone.
- **Filtering:** Use the search box to filter influences by name. Wildcards and regular
  expressions both work. You can also hide unused and locked columns.
- **Copy / Paste** Copy weights from one set of vertices to another. Each pasted vertex
  takes the weights of the closest copied vertex, using the undeformed mesh.
- **Problem Verts** Selects vertices whose edges stretch more than the given factor
  (4 by default) compared to the undeformed mesh. That usually points to bad weights.

## Scripting with blurSkinCmd

The `blurSkin` plugin also provides `blurSkinCmd`, an undoable command for weight operations
from scripts:

```python
from maya import cmds

# Smooth two vertices of a mesh, three times
cmds.blurSkinCmd(command="smooth", meshName="bodyShape", listVerticesIndices=[12, 13], repeat=3)

# Prune small weights on the whole mesh
cmds.blurSkinCmd(command="prune", meshName="bodyShape", threshold=0.01)

# Print the full list of flags
cmds.blurSkinCmd(help=True)
```

The `command` flag can be `smooth`, `add`, `absolute`, `percentage`, `average`, `colors`
or `prune`. If you don't pass `meshName` or `skinCluster`, the command uses the selection,
including soft selection.

## Development

- `src/blurSkin` and `src/brSkinBrush` hold the two C++ plugins, and `scripts/` holds the
  Python tools.
- Building: `quick_compile.bat` builds and installs a debug build. 
  Edit `MAYA_VERSION` and `BUILDTYPE` at the top of it for other versions or a release build.
- Tests: `tests\run_tests.bat` builds the plugins, runs the C++ unit tests, and then runs the
  mayapy integration tests. See [tests/README.md](tests/README.md) for the one-time setup.
- Reloading: `tools/reloadPlugin.py` and the `tools/shelf_blurWeightModule_DEV.mel` shelf
  reload the plugins and tools without restarting Maya.

### Updating the screenshots

The images in `docs/images` are generated via script. After building, run `tools\capture_screenshots.bat`

This opens Maya with a temporary preferences folder, so your own prefs aren't touched. It
builds a test scene, opens each tool and saves the screenshots, then quits Maya. The steps
are in `tools/capture_screenshots.py`. You may need to install numpy/Qt.py manually for this to work.
