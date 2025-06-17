# Vulkan
vulkan_dep = dependency('vulkan', required: true)

# GLFW
glfw_dep = dependency('glfw3', required: true, fallback: ['glfw', 'glfw_dep'])

# ImGui
imgui_dep = dependency('imgui', fallback: ['imgui', 'imgui_dep'])

link_args = []

if host_system == 'linux'
  if get_option('build_for_musl') == true
    link_args += ['-static']
  else
    link_args += ['-static-libgcc', '-static-libstdc++']
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