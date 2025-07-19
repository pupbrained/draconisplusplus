# Vulkan
vulkan_dep = dependency('vulkan', required: true)

# GLFW
glfw_dep = dependency('glfw3', required: true, fallback: ['glfw', 'glfw_dep'])

# ImGui
imgui_dep = dependency('imgui', fallback: ['imgui', 'imgui_dep'])
glaze_dep = dependency('glaze', required: false)

link_args = []

if host_system == 'linux'
  if get_option('build_for_musl') == true
    link_args += ['-static']
  else
    link_args += ['-static-libgcc', '-static-libstdc++']
  endif
endif

# Nintendo Switch (Horizon OS) example
# Build Nintendo Switch .nro when devkitPro toolchain is available

switch_example = get_option('build_switch_example')

if switch_example
  aarch64_gpp = find_program('aarch64-none-elf-g++', required: switch_example)
  elf2nro = find_program('elf2nro', required: switch_example)
  nacptool = find_program('nacptool', required: switch_example)
  devkitpro = run_command('sh', '-c', 'printf %s "$DEVKITPRO"', check: false).stdout().strip()

  if aarch64_gpp.found() and elf2nro.found() and nacptool.found() and devkitpro != ''
    specs_arg = '-specs=' + devkitpro + '/libnx/switch.specs'
    include_arg = '-I' + devkitpro + '/libnx/include'
    lib_arg = '-L' + devkitpro + '/libnx/lib'
    proj_inc_arg = '-I' + meson.project_source_root() + '/include'
    third_party_inc_arg = '-I' + meson.project_source_root() + '/third_party'
    src_inc_arg = '-I' + meson.project_source_root() + '/src'
    lib_inc_arg = '-I' + meson.project_source_root() + '/src/Lib'
    glaze_inc_arg = ''
    if glaze_dep.found()
      glaze_inc_dirs = glaze_dep.get_variable(pkgconfig: 'includedir', default_value: '')
      if glaze_inc_dirs != ''
        glaze_inc_arg = '-I' + glaze_inc_dirs
      endif
    endif
    extra_glaze_inc = glaze_inc_arg == '' ? [] : [glaze_inc_arg]

    nix_cflags = run_command('sh', '-c', 'printf %s "$NIX_CFLAGS_COMPILE"', check: false).stdout().strip().split()

    switch_nacp = custom_target(
      'switch_app_nacp',
      output: 'switch_app.nacp',
      command: [
        nacptool,
        '--create', 'switch_app', # Title
        'draconis++', # Author
        meson.project_version(),
        '@OUTPUT@',
      ],
      build_by_default: true,
      install: false,
    )

    switch_src = [
      'switch_app/main.cpp',
      meson.project_source_root() / 'src/Lib/OS/Switch.cpp',
    ]

    # Add weather service sources if weather is enabled
    if get_option('weather').enabled()
      switch_src += [
        meson.project_source_root() / 'src/Lib/Services/Weather/MetNoService.cpp',
        meson.project_source_root() / 'src/Lib/Services/Weather/OpenMeteoService.cpp',
        meson.project_source_root() / 'src/Lib/Services/Weather/OpenWeatherMapService.cpp',
        meson.project_source_root() / 'src/Lib/Services/Weather/WeatherFactory.cpp',
        meson.project_source_root() / 'src/Lib/Services/Weather/WeatherUtils.cpp',
      ]
    endif

    # Add weather feature flag and dependencies for Switch app
    weather_flag = get_option('weather').enabled() ? ['-DDRAC_ENABLE_WEATHER=1'] : []

    # Add Switch-specific defines for weather
    if get_option('weather').enabled()
      weather_flag += [
        '-D_DEFAULT_SOURCE', # Expose timegm on some platforms
        '-DDRAC_VERSION="' + meson.project_version() + '"',
      ]
    endif

    # Add libcurl for weather services if enabled
    weather_libs = []
    if get_option('weather').enabled()
      # Try to find curl in devkitPro or common Switch development locations
      curl_lib_paths = [
        devkitpro + '/portlibs/switch/lib',
        devkitpro + '/libnx/lib',
        '/opt/devkitpro/portlibs/switch/lib',
        '/opt/devkitpro/libnx/lib',
      ]

      curl_found = false
      foreach path : curl_lib_paths
        if not curl_found and run_command('test', '-f', path + '/libcurl.a', check: false).returncode() == 0
          weather_libs += ['-L' + path, '-lcurl']
          curl_found = true
        endif
      endforeach

      if not curl_found
        # Fallback to standard curl dependency
        curl_dep = dependency('libcurl', required: false)
        if curl_dep.found()
          weather_libs += ['-lcurl']
        else
          # Last resort: try standard library path
          weather_libs += ['-lcurl']
        endif
      endif

      # Add zlib dependency for curl compression support
      zlib_lib_paths = [
        devkitpro + '/portlibs/switch/lib',
        devkitpro + '/libnx/lib',
        '/opt/devkitpro/portlibs/switch/lib',
        '/opt/devkitpro/libnx/lib',
      ]

      zlib_found = false
      foreach path : zlib_lib_paths
        if not zlib_found and run_command('test', '-f', path + '/libz.a', check: false).returncode() == 0
          weather_libs += ['-L' + path, '-lz']
          zlib_found = true
        endif
      endforeach

      if not zlib_found
        # Fallback to standard zlib dependency
        zlib_dep = dependency('zlib', required: false)
        if zlib_dep.found()
          weather_libs += ['-lz']
        else
          # Last resort: try standard library path
          weather_libs += ['-lz']
        endif
      endif
    endif

    switch_elf = custom_target(
      'switch_app_elf',
      input: switch_src,
      output: 'switch_app.elf',
      command: [
        aarch64_gpp,
        '@INPUT@',
        '-o', '@OUTPUT@',
        '-std=gnu++23',
        '-D__SWITCH__', proj_inc_arg,
        '-Dfn=auto',
        third_party_inc_arg,
        src_inc_arg,
        lib_inc_arg,
      ]
      + extra_glaze_inc
      + nix_cflags
      + weather_flag
      + weather_libs
      + [
        '-g',
        '-O2',
        '-ffunction-sections',
        '-fPIE',
        '-march=armv8-a+crc+crypto',
        '-mtune=cortex-a57',
        '-mtp=soft',
        include_arg,
        specs_arg,
        lib_arg,
        '-lnx',
      ],
      build_by_default: true,
      install: false,
    )

    custom_target(
      'switch_app_nro',
      input: [switch_elf, switch_nacp],
      output: 'switch_app.nro',
      command: [elf2nro, '@INPUT0@', '@OUTPUT@', '--nacp=@INPUT1@'],
      build_by_default: true,
      install: false,
    )
  elif switch_example
    error('build_switch_example enabled but required tools not found')
  endif
endif

executable(
  'vulkan_app',
  'vulkan_app/main.cpp',
  dependencies: [draconis_dep, vulkan_dep, glfw_dep, imgui_dep] + lib_deps,
  link_args: link_args,
  install: false,
)

executable(
  'glaze_http',
  'glaze_http/main.cpp',
  dependencies: [draconis_dep] + lib_deps,
  link_args: link_args,
  install: false,
)