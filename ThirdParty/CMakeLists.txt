find_package(Vulkan REQUIRED)

add_subdirectory(glfw EXCLUDE_FROM_ALL)

if (WIN32)
   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif(WIN32)
add_subdirectory(volk)

add_subdirectory(meshoptimizer EXCLUDE_FROM_ALL)

add_library(stb STATIC stb_impl.cpp)
target_include_directories(stb PUBLIC stb)

add_library(imgui STATIC)
target_include_directories(imgui PUBLIC imgui)
target_compile_definitions(imgui PUBLIC -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES) # Tell imgui to don't load vulkan functions
target_link_libraries(imgui PUBLIC glfw volk)
target_sources(imgui PRIVATE 
  imgui/imgui.h
  imgui/imgui.cpp

  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_tables.cpp
  imgui/backends/imgui_impl_vulkan.cpp
  imgui/backends/imgui_impl_glfw.cpp
)