cmake_minimum_required(VERSION 3.10)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

project (VulkanRenderer)

# ouput exe to bin directory
#SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
#foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
#    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
#    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin )
#endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

#set(CMAKE_DEBUG_POSTFIX d)

# ThirdParty dependencies
find_package(Vulkan REQUIRED)
add_subdirectory(ThirdParty/glfw)

if (WIN32)
   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif(WIN32)
add_subdirectory(ThirdParty/volk)

add_subdirectory(ThirdParty/meshoptimizer)

#set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})


add_subdirectory(Sources)