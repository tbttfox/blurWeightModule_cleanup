# blurWeightModule — Test Suite

## What this is / who this is for

A standalone test suite for the two C++ plugins (`blurSkin`, `brSkinBrush`)
and the two Python tools (`mWeightEditor`, `mPaintEditor`) in this repo. It
is **not** wired into the main `meson.build` or into CI — it's meant to be
run locally by a developer to check their changes. If you're just using the
tools day to day, you can ignore this folder entirely.

## Prerequisites

- Maya 2026 installed at `C:/Program Files/Autodesk/Maya2026` (adjust paths
  below if yours is elsewhere).
- The same build environment `quick_compile.bat` already needs: Visual
  Studio + `meson` + `ninja` on `PATH`.
- (First time only) install `pytest` into mayapy, and install Catch2 for the
  C++ tests — see "One-time setup" below.

## Quick start (TL;DR)

From the repo root:
```
tests\run_tests.bat
```
This builds the plugins, builds and runs the C++ tests, then runs the
Python/mayapy tests, all in one go.

## One-time setup

### 1. Install pytest into mayapy's Python

mayapy ships its own Python interpreter and its own `pip` — packages
installed for your regular `python.exe` are **not** visible to it.
```
"C:\Program Files\Autodesk\Maya2026\bin\mayapy.exe" -m pip install -r tests\requirements-test.txt
```
If this fails with a permissions error (it's writing into `Program Files`),
either run your terminal as Administrator, or add `--user` to the command.

### 2. Install Catch2 for the C++ tests

Catch2 is the C++ test framework used here. It's fetched the same way this
repo already fetches `rapidjson` (see `subprojects/rapidjson.wrap`) — via
Meson's WrapDB:
```
cd tests\cpp
meson wrap install catch2
cd ..\..
```
You only need to do this once; the downloaded wrap is cached under
`tests/cpp/subprojects/`.

## Running just the C++ tests

From the repo root:
```
meson setup tests\cpp\cppbuild tests\cpp --vsenv
meson compile -C tests\cpp\cppbuild
meson test -C tests\cpp\cppbuild
```
(Skip `meson setup` on subsequent runs — only `compile`/`test` are needed
unless you delete `tests\cpp\cppbuild\`.)

To run a single test executable directly (useful for filtering by name):
```
tests\cpp\cppbuild\test_blurSkin_functions.exe "CVsAround*"
```

## Running just the Maya/Python tests

The plugins must be built first (`quick_compile.bat`, or step 1 of
`run_tests.bat`) so the `.mll` files exist for `cmds.loadPlugin(...)` to find.
Then:
```
"C:\Program Files\Autodesk\Maya2026\bin\mayapy.exe" -m pytest tests\mayapy -v
```
To run a single test file or test:
```
"C:\Program Files\Autodesk\Maya2026\bin\mayapy.exe" -m pytest tests\mayapy\test_blurskin_cmd.py -v
```

## How this test suite is organized

### `tests/cpp` — pure weight-math functions, no Maya session needed

Functions like `CVsAround`, `getMIntArrayIndex`, `lineC`, `distance` in
`src/blurSkin/src/functions.cpp` and `src/brSkinBrush/src/functions.cpp`
only operate on simple Maya data types (`MIntArray`, `MDoubleArray`, tuples
of floats) that work fine without a running Maya session. `tests/cpp/`
compiles those *exact same* `.cpp` files into small Catch2 test executables
— nothing is copied or reimplemented, so there's no risk of the tests
drifting out of sync with the real plugin code.

### `tests/mayapy` — real Maya behavior, via a headless Maya session

Everything else (the actual `blurSkinCmd` command, and the
`mWeightEditor`/`mPaintEditor` Python tools that read/write real
skinClusters) needs a live Maya scene to mean anything. `mayapy.exe` runs a
real, licensed-GUI-free Maya Python interpreter. `tests/mayapy/conftest.py`
starts that session at import time (before pytest collects any test module),
loads the compiled plugins, and gives every test function a fresh empty
scene. `brSkinBrushCmd` is not covered here — see "out of scope" below.

### `helpers/scene_fixtures.py` — the reusable "build a test rig" helper

`build_sphere_with_skincluster()` builds a poly sphere bound to a couple of
joints via a real `skinCluster`. Nearly every mayapy test starts by calling
this to get something realistic to operate on.

## Troubleshooting

- **`loadPlugin` failed in conftest.py`** — the compiled `.mll` files don't
  exist yet, or `MAYA_PLUGIN_PATH` doesn't point at them. Run
  `quick_compile.bat` first, and check that
  `modules\blurWeightModule\win64-2026\plug-ins\blurSkin.mll` exists.
- **`ModuleNotFoundError: No module named 'pytest'`** — you ran plain
  `python.exe` instead of `mayapy.exe`. All the Python test files import
  `maya`, so they only work under mayapy.
- **`Foundation.lib not found` (or similar linker error) building
  `tests/cpp`** — check the `maya_install_path` option in
  `tests/cpp/meson.build` matches where Maya 2026 is actually installed
  (`meson configure tests\cpp\cppbuild -Dmaya_install_path=...` to change it
  without re-running `meson setup`).
- **`LNK2019: unresolved external symbol` mentioning `MFnSkinCluster` or
  `MFnGeometryFilter`** — compiling a whole `functions.cpp` file pulls in
  symbols for every function it defines, not just the ones a test calls, so
  `tests/cpp/meson.build` links against the same full library set
  (`Foundation`, `OpenMaya`, `OpenMayaAnim`, `OpenMayaFX`, `OpenMayaRender`,
  `OpenMayaUI`, `clew`) that the real plugins do. If you add a test for a
  function in a new source file that needs a library not already listed
  there, add it the same way.
- **`Could not find Maya QT headers` when building the plugins themselves**
  (via `quick_compile.bat`) — the Maya devkit ships its Qt headers as a zip
  (`C:\Program Files\Autodesk\Maya2026\include\qt_*-include.zip`) that needs
  to be extracted once, into that same folder, from an elevated terminal
  (it writes into `Program Files`). This is a one-time Maya devkit setup
  step, unrelated to this test suite.
- **`pip install` permission denied** — use `--user`, or run the terminal as
  Administrator.

## What's out of scope for v1 (and why)

- **`brSkinBrushCmd`/`brSkinBrushContext` entirely** — not just interactive
  drag-painting, but *creating the tool context at all*. Confirmed
  experimentally: even Maya's own built-in tool contexts (e.g.
  `manipMoveContext`) return `False` and fail to register under
  `maya.standalone` — `MPxContext`/context-command creation needs a live,
  interactive Maya session (tool manager), not just a viewport for mouse
  events. The `-flood` flag would otherwise be a good headless substitute
  for drag-painting, but it still requires a context to exist first, so it
  can't be reached at all from mayapy. There is currently no way to test
  this plugin's command surface headlessly; the `tests/cpp` pure-logic
  tests are this plugin's only automated coverage for v1.
- **`meshFnIntersection.py`'s `Orbit` class** (mPaintEditor) — calls
  `OpenMayaUI.M3dView.active3dView()` directly, which requires a real
  viewport. Not feasible headlessly at all, so it's excluded rather than
  deferred.
- **Qt widget tests** (`tableWidget.py`, `spinnerSlider.py`,
  `catchEventsUI.py`, etc.) — testing Qt widgets needs a running
  `QApplication` event loop, a materially heavier setup than anything else
  here. Possible v2 using `pytest-qt` and Qt's `offscreen` platform plugin.
- **`blurSkinDisplay`/`pointsDisplay` nodes** — paint-feedback display nodes
  with custom viewport draw overrides. There's no scripted entry point like
  `-flood` for these, and correctness is inherently visual.
- **`brushPythonFunctions.py`** (mPaintEditor) — mostly session/UI-state
  plumbing (undo contexts, optionVar toggling, nurbs-tessellate helpers)
  tied to a live tool context. Lower value than the four subsystems covered
  here; add later using the same `conftest.py` pattern if needed.
- **Remaining pure-logic C++ functions** — `editArray`, `setAverageWeight`,
  `doPruneWeight`, `getMirrorVertices`, `findClosestWithinThreshold`, the
  bbox/ray geometry helpers, `getRawNeighbors`, `editArrayMirror`. Same
  technique as the functions already covered, just more test-data setup per
  function — add them the same way when you need more coverage.
- **CI integration** — this suite is meant to run locally for now. Wiring it
  into `.github/workflows/main.yml` is a reasonable follow-up once it's
  proven out, but requires a CI image with a real Maya/mayapy install, which
  the current devkit-only CI setup doesn't have.

## Adding a new test

- **C++**: add a new `TEST_CASE("...", "[pluginName]") { REQUIRE(...); }` to
  the relevant `tests/cpp/test_*.cpp` file (or create a new one and add it
  to `tests/cpp/meson.build`).
- **Python/Maya**: add a new `test_*.py` file under `tests/mayapy/`, using
  `from helpers.scene_fixtures import build_sphere_with_skincluster` to get
  a scene to work with. Look at the existing test files for the pattern.
