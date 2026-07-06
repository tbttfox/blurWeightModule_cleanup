from typing import Optional, Union, Callable, Any
import os
import sys
import shutil
import time
from pathlib import Path
from maya import cmds

RELEASE_TYPES = [
    "Debug",
    "RelWithDebInfo",
    "Release",
    "debug",
    "debugoptimized",
    "release",
]

EXTS = {
    "win32": ".mll",
    "linux": ".so",
    "linux2": ".so",
    "darwin": ".bundle",
}


def _findSrcAndReleaseType(
    folder: Path,
    srcFmt: str,
    year: str,
    plugName: str,
    overrides: dict[str, str],
) -> Path:
    """Find the most recently compiled plugin in all release types"""

    ext = EXTS[sys.platform]

    tups = []
    for rt in RELEASE_TYPES:
        src = folder / srcFmt.format(
            releaseType=rt, year=year, plugName=plugName, **overrides
        )
        path = src / f"{plugName}{ext}"
        if not path.exists():
            continue
        tups.append((path.stat().st_mtime, src))

    if not tups:
        raise IOError("Cannot find a valid .mll plugin")
    _mtime, src = max(tups)
    return src


def _findSrc(
    folder: Path,
    srcFmt: str,
    plugName: str,
    releaseType: Optional[str],
    year: str,
    overrides: dict[str, str],
) -> Path:
    if releaseType is None:
        return _findSrcAndReleaseType(folder, srcFmt, year, plugName, overrides)
    return folder / srcFmt.format(releaseType=releaseType, year=year, **overrides)


def _makeDst(folder: Path, year: str) -> Path:
    # Maya locks the mll files on windowsr, So instead of overwriting
    # I have to create new folders for the loaded plugins
    dpar = folder / "loaded"
    for i in range(200):
        newdst = dpar / f"{year}_{i:03d}"
        if not newdst.is_dir():
            newdst.mkdir(parents=True)
            return newdst
    raise RuntimeError("You've made over 200 iterations. Clean up after yourself")


def reloadPlugin(
    folder: Union[str, Path],
    plugNames: Union[str, list[str]],
    releaseType: Optional[str] = None,
    openFile: Optional[str] = None,
    openFunc: Optional[Callable[[], Any]] = None,
    srcFmt: str = "mayabuild_{year}/{releaseType}",
    src: Optional[
        Union[str, Path, list[str], list[Path], tuple[str, ...], tuple[Path, ...]]
    ] = None,
    dst: Optional[Union[str, Path]] = None,
    overrides: Optional[dict[str, str]] = None,
):
    """
    Close the current file, make a copy of the plugin file you specify, and load it
    taking into account the current maya version and platform

    Parameters
    ----------
    folder: str
        The base folder to look for the plugin. Use the `srcFmt` and `dstFmt` to
        find the actual plugins
    plugNames: str or list[str]
        The name of the plugin, without the platform specific suffix (like .mll .so or .bundle)
    releaseType: str (optional)
        The releaseType from (Debug, RelWithDebInfo, Release). If this is not specified
        all three types are checked, and the latest one is loaded
    openFile: str (optional)
        If specified, open this file once the plugin is reloaded
    openFunc: callable (optional)
        If specified, run this function once the plugin is reloaded
    src: str or list[str] (optional)
        If provided, override the the source plugin file to copy
    dst: str (optional)
        If provided, Copy the source to this location to load from
    overrides: dict (optional)
        Provide any extra keyword arguments for the srcFmt or dstFmt
    """
    overrides = overrides or {}
    year: str = cmds.about(version=True)
    if isinstance(plugNames, str):
        plugNames = [plugNames]
    folder = Path(folder)

    if dst is None:
        dst = _makeDst(folder, year)
    dst = Path(dst)

    cmds.file(force=True, newFile=True)
    curext = EXTS[sys.platform]

    all_exts = ["", curext, ".py"] + list(EXTS.values())
    for plugName in plugNames:
        for ext in all_exts:
            pname = f"{plugName}{ext}"
            print("Attempting Unloading Plugin:", pname)
            unloaded = cmds.unloadPlugin(pname, force=True)
            if unloaded:
                print("Unloaded:", pname)
                break

    psd: list[tuple[str, Path]] = []
    if src is None:
        for plugName in plugNames:
            plugSrc = _findSrc(folder, srcFmt, plugName, releaseType, year, overrides)
            psd.append((plugName, plugSrc))
    else:
        if isinstance(src, (str, Path)):
            sx = Path(src)
            psd = [(pn, sx) for pn in plugNames]
        elif isinstance(src, (list, tuple)):
            if len(src) != len(plugNames):
                raise ValueError(
                    "The number of plug names must match the number of sources"
                )
            spaths = [Path(s) for s in src]
            psd = list(zip(plugNames, spaths))

    # Let the operating system catch up
    time.sleep(0.5)

    toLoad = []
    for plugName, plugSrc in psd:
        if not plugSrc or not plugSrc.is_dir():
            continue

        print("Copying from", plugSrc)
        for p in plugSrc.iterdir():
            if p.suffix == curext:
                toLoad.append(dst / p.name)
                shutil.copy(p, dst)

            if p.suffix in (".cl", ".ilk", ".lib", ".exp", ".pdb"):
                shutil.copy(p, dst)

        clPath = folder / f"{plugName}.cl"
        if clPath.is_file():
            shutil.copy(clPath, dst)

    for loader in set(toLoad):
        print("Loading Plugin", loader)
        cmds.loadPlugin(loader)

    if openFile is not None:
        typ = "mayaBinary"
        if openFile[-1] == "a":
            typ = "mayaAscii"
        cmds.file(openFile, force=True, typ=typ, o=True)

    if openFunc is not None:
        openFunc()
