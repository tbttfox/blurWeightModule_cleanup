project('blurWeightModule', 'cpp', default_options: ['cpp_std=c++20'])

fs = import('fs')
maya_dep = dependency('maya')
maya_name_suffix = maya_dep.get_variable('name_suffix')
maya_version = maya_dep.get_variable('maya_version')
maya_platform = maya_dep.get_variable('maya_platform')
ci_install = get_option('ci_install')

if ci_install
  plugin_install_dir = meson.global_source_root() / ('output_Maya' + maya_version)
else
  plugin_install_dir = meson.global_source_root() / 'modules' / meson.project_name() / (maya_platform + '-' + maya_version) / 'plug-ins'
endif

subdir('src/blurSkin')
subdir('src/brSkinBrush')

# The CI packaging step copies the scripts and writes the .mod file itself
if ci_install
  subdir_done()
endif

pyinstall_dir = meson.project_source_root() / 'modules' / meson.project_name()
install_subdir(
    'scripts',
    install_dir: pyinstall_dir,
    exclude_files: ['ruff.toml'],
)

# Define configuration data
conf = configuration_data(
    {
        'PROJNAME': meson.project_name(),
        'FOLDERNAME': meson.project_name(),
        'VERSION': 'v1.0.0',
        'MAYA_VERSION': maya_version,
        'MAYA_PLATFORM': maya_platform,
    },
)
configure_file(
    input: 'module.mod.in',
    output: meson.project_name() + '.mod',
    configuration: conf,
    install_dir: meson.global_source_root() / 'modules',
    format: 'cmake',
)