project('blurWeightModule-cpp-tests', 'cpp', default_options: ['cpp_std=c++20'])

maya_install_path = get_option('maya_install_path')
cpp = meson.get_compiler('cpp')

maya_lib_dir = maya_install_path / 'lib'

# Matching the full library set the real plugins link against
# (see subprojects/maya/meson.build): compiling functions.cpp as a whole
# translation unit pulls in symbols from every function it defines, not
# just the ones our tests call, so all of these are needed at link time.
maya_dep = declare_dependency(
  include_directories: include_directories(maya_install_path / 'include'),
  dependencies: [
    cpp.find_library('Foundation', dirs: maya_lib_dir),
    cpp.find_library('OpenMaya', dirs: maya_lib_dir),
    cpp.find_library('OpenMayaAnim', dirs: maya_lib_dir),
    cpp.find_library('OpenMayaFX', dirs: maya_lib_dir),
    cpp.find_library('OpenMayaRender', dirs: maya_lib_dir),
    cpp.find_library('OpenMayaUI', dirs: maya_lib_dir),
    cpp.find_library('clew', dirs: maya_lib_dir),
  ],
  compile_args: ['-DREQUIRE_IOSTREAM', '-D_BOOL', '-DNT_PLUGIN'],
)
catch2_dep = dependency('catch2-with-main')

blurskin_test_exe = executable(
  'test_blurSkin_functions',
  [
    'test_blurSkin_functions.cpp',
    '../../src/blurSkin/src/functions.cpp',
  ],
  include_directories: include_directories('../../src/blurSkin/include'),
  dependencies: [maya_dep, catch2_dep],
)
test('blurSkin pure-logic functions', blurskin_test_exe)

brskinbrush_test_exe = executable(
  'test_brSkinBrush_functions',
  [
    'test_brSkinBrush_functions.cpp',
    '../../src/brSkinBrush/src/functions.cpp',
  ],
  include_directories: include_directories('../../src/brSkinBrush/include'),
  dependencies: [maya_dep, catch2_dep],
)
test('brSkinBrush pure-logic functions', brskinbrush_test_exe)
