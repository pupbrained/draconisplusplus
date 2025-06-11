# examples/meson.build

# Vulkan
vulkan_dep = dependency('vulkan', required: true)

# GLFW
glfw_dep = dependency('glfw3', required: true, fallback: ['glfw', 'glfw_dep'])

# ImGui
# ImGui is often included as source, but we'll try to find it as a dependency first.
# If this fails, we will need to add it as a subproject.
imgui_dep = dependency('imgui', fallback: ['imgui', 'imgui_dep'])

executable(
  'vulkan_example',
  'vulkan_example/main.cpp',
  dependencies: [vulkan_dep, glfw_dep, imgui_dep] + lib_deps,
  include_directories: [project_internal_includes, project_public_includes],
  cpp_args: common_cpp_args,
  link_with: [lib],
  install: false,
)