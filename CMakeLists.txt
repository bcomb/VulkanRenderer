cmake_minimum_required(VERSION 3.10)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

project (VulkanRenderer)

# Debug target
set(CMAKE_DEBUG_POSTFIX d)

# Ouput exe/lib to bin directory
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin)
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

# ThirdParty dependencies
find_package(Vulkan REQUIRED)
add_subdirectory(ThirdParty/glfw)
if (WIN32)
   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif(WIN32)
add_subdirectory(ThirdParty/volk)
add_subdirectory(ThirdParty/meshoptimizer)

add_subdirectory(Sources/VulkanCore)

# Examples
add_subdirectory(Sources/Examples/Simple)
add_subdirectory(Sources/Examples/FirstCompute)

#set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Simple)