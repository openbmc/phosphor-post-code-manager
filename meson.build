project(
    'post-code-manager',
    'cpp',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
        'werror=true',
    ],
    license: 'Apache-2.0',
    meson_version: '>=0.57.0',
    version: '1.0',
)


conf_data = configuration_data()
conf_data.set_quoted('DBUS_OBJECT_NAME', 'xyz/openbmc_project/State/Boot/PostCode0')
conf_data.set_quoted('DBUS_INTF_NAME','xyz.openbmc_project.State.Boot.PostCode')
conf_data.set('MAX_BOOT_CYCLE_COUNT',get_option('max-post-code-entries'))

if get_option('bios-post-code-log').enabled()
  add_project_arguments('-DENABLE_BIOS_POST_CODE_LOG',language: 'cpp')
endif

configure_file(output: 'config.h',
  configuration: conf_data
)

sdbusplus = dependency(
    'sdbusplus',
    fallback: ['sdbusplus', 'sdbusplus_dep']
)

phosphor_logging = dependency(
    'phosphor-logging',
    fallback: [
        'phosphor-logging',
        'phosphor_logging_dep'])

phosphor_dbus_interfaces = dependency(
  'phosphor-dbus-interfaces',
  fallback: ['phosphor-dbus-interfaces', 'phosphor_dbus_interfaces_dep']
)

cxx = meson.get_compiler('cpp')
cereal_dep = dependency('cereal', required: false)
has_cereal = cxx.has_header_symbol(
    'cereal/cereal.hpp',
    'cereal::specialize',
    dependencies: cereal_dep,
    required: false)
if not has_cereal
    cereal_opts = import('cmake').subproject_options()
    cereal_opts.add_cmake_defines({'BUILD_TESTS': 'OFF'})
    cereal_proj = import('cmake').subproject(
        'cereal',
        options: cereal_opts,
        required: false)
    assert(cereal_proj.found(), 'cereal is required')
    cereal_dep = cereal_proj.dependency('cereal')
endif

install_subdir('service_files',
               install_dir : get_option('prefix')+'/lib/systemd/system/',
               strip_directory : true)

executable(
    'post-code-manager',
    'src/main.cpp',
    'src/post_code.cpp',
    install: true,
    dependencies: [
      sdbusplus,
      phosphor_dbus_interfaces,
      phosphor_logging,
      cereal_dep],
    include_directories: 'inc')