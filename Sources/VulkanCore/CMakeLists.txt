project (VulkanCore)

set(sources
    main.cpp
    Application.h Application.cpp
    Window.h Window.cpp
    vk_common.h
    Shaders.h Shaders.cpp
    VulkanContext.h VulkanContext.cpp
    VulkanDevice.h VulkanDevice.cpp
    VulkanBuffer.h VulkanBuffer.cpp
    VulkanImage.h VulkanImage.cpp
    VulkanHelper.h VulkanHelper.cpp    
    VulkanSwapchain.h VulkanSwapchain.cpp)

source_group("Sources" FILES ${sources})

file(GLOB glsl_sources "../Shaders/*.glsl")
file(GLOB glsl_inc "../Shaders/*.h")
source_group(Shaders FILES ${glsl_sources} ${glsl_inc})

#add_executable(${PROJECT_NAME} ${sources} ${glsl_sources})
add_library(${PROJECT_NAME} STATIC ${sources} ${glsl_sources})

IF(WIN32)
        target_compile_definitions(${PROJECT_NAME} PUBLIC -DVK_USE_PLATFORM_WIN32_KHR)
        target_compile_definitions(${PROJECT_NAME} PUBLIC -DGLFW_EXPOSE_NATIVE_WIN32)
        target_compile_definitions(${PROJECT_NAME} PUBLIC -D_CRT_SECURE_NO_WARNINGS) # fopen anoying warning
        target_compile_definitions(${PROJECT_NAME} PUBLIC -DWIN32_LEAN_AND_MEAN)  # reduce number of include on windows header
        target_compile_definitions(${PROJECT_NAME} PUBLIC -DNOMINMAX)
ENDIF(WIN32)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_HOME_DIRECTORY}/ThirdParty/stb) # to access stb
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_HOME_DIRECTORY}/ThirdParty/VulkanMemoryAllocator/include) # to access Vma

target_link_libraries (${PROJECT_NAME} PUBLIC glfw volk meshoptimizer)
#set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")
#set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

# This is to propagate the include directories to project that link with this one
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# For HotReload
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/EDITANDCONTINUE /INCREMENTAL")
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/ZI")
endif()

foreach(fileItem ${glsl_sources})
    add_custom_command( OUTPUT ${fileItem}.spv
                        POST_BUILD                        
                        COMMAND glslangValidator.exe -V "${fileItem}" -o "${fileItem}.spv"
                        MAIN_DEPENDENCY ${fileItem})
endforeach(fileItem)