project(
    'post-code-manager',
    'cpp',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
        'werror=true',
    ],
    license: 'Apache-2.0',
    meson_version: '>=0.58.0',
    version: '1.0',
)


conf_data = configuration_data()
conf_data.set_quoted('DBUS_OBJECT_NAME', '/xyz/openbmc_project/State/Boot/PostCode')
conf_data.set_quoted('DBUS_INTF_NAME','xyz.openbmc_project.State.Boot.PostCode')
conf_data.set_quoted('POSTCODE_DISPLAY_PATH', get_option('postcode-display-path'))
conf_data.set('MAX_BOOT_CYCLE_COUNT',get_option('max-boot-cycle-count'))
conf_data.set('MAX_POST_CODE_SIZE_PER_CYCLE',get_option('max-post-code-size-per-cycle'))

if get_option('bios-post-code-log').enabled()
  add_project_arguments('-DENABLE_BIOS_POST_CODE_LOG',language: 'cpp')
endif

configure_file(output: 'config.h',
  configuration: conf_data
)

sdbusplus = dependency('sdbusplus')
phosphor_logging = dependency('phosphor-logging')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')

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

systemd_system_unit_dir = dependency('systemd').get_variable(
          'systemdsystemunitdir')

install_subdir('service_files',
               install_dir : systemd_system_unit_dir,
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
