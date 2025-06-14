# Vulkan
vulkan_dep = dependency('vulkan', required: true)

# GLFW
glfw_dep = dependency('glfw3', required: true, fallback: ['glfw', 'glfw_dep'])

# ImGui
# ImGui is often included as source, but we'll try to find it as a dependency first.
# If this fails, we will need to add it as a subproject.
imgui_dep = dependency('imgui', fallback: ['imgui', 'imgui_dep'])

# Linker/ObjC configuration for the example executable
link_args = []
objc_args = []

if host_system == 'darwin'
  objc_args += ['-fobjc-arc']
elif host_system == 'linux'
  if get_option('build_for_musl') == true
    link_args += ['-static']
  else
    link_args += ['-static-libgcc', '-static-libstdc++']
  endif
elif host_system == 'haiku'
  link_args += ['-lpackage', '-lbe']
endif

executable(
  'vulkan_example',
  'vulkan_example/main.cpp',
  dependencies: [draconis_dep, vulkan_dep, glfw_dep, imgui_dep] + lib_deps,
  link_args: link_args,
  objc_args: objc_args,
  install: false,
)
