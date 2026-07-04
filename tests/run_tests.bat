setlocal
SET MAYAPY=C:\Program Files\Autodesk\Maya2026\bin\mayapy.exe

echo === Step 1: Building C++ plugins ===
call quick_compile.bat

echo === Step 2: Building + running C++ pure-logic tests ===
if not exist tests\cpp\cppbuild\ (
    meson setup tests\cpp\cppbuild tests\cpp --vsenv
)
meson compile -C tests\cpp\cppbuild
meson test -C tests\cpp\cppbuild

echo === Step 3: Running mayapy/pytest tests ===
"%MAYAPY%" -m pytest tests\mayapy -v

pause
