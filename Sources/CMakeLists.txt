project (rendererVK)

set(sources
    main.cpp
    vk_common.h
    Shaders.h Shaders.cpp
    VulkanContext.h VulkanContext.cpp
    VulkanDevice.h VulkanDevice.cpp
    VulkanHelper.h VulkanHelper.cpp
    VulkanSwapchain.h VulkanSwapchain.cpp)

source_group("Sources" FILES ${sources})

IF(WIN32)
        add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
        add_definitions(-DGLFW_EXPOSE_NATIVE_WIN32)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS) # fopen anoying warning
        add_definitions(-DWIN32_LEAN_AND_MEAN)  # reduce number of include on windows header
        add_definitions(-DNOMINMAX)
ENDIF(WIN32)

file(GLOB glsl_sources "../Shaders/*.glsl")
file(GLOB glsl_inc "../Shaders/*.h")
source_group(Shaders FILES ${glsl_sources} ${glsl_inc})


add_executable(${PROJECT_NAME} ${sources} ${glsl_sources})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/stb) # to access stb
target_link_libraries (${PROJECT_NAME} PRIVATE glfw volk meshoptimizer)
#set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")

foreach(fileItem ${glsl_sources})
    add_custom_command( OUTPUT ${fileItem}.spv
                        POST_BUILD                        
                        COMMAND glslangValidator.exe -V "${fileItem}" -o "${fileItem}.spv"
                        MAIN_DEPENDENCY ${fileItem})
endforeach(fileItem)