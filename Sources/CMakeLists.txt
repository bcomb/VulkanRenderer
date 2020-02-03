project (rendererVK)

set(sources
        main.cpp)

source_group("Sources" FILES ${sources})

IF(WIN32)
        add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
        add_definitions(-DGLFW_EXPOSE_NATIVE_WIN32)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS) # fopen anoying warning
        add_definitions(-DWIN32_LEAN_AND_MEAN)  # reduce number of include on windows header
        add_definitions(-DNOMINMAX)

ENDIF(WIN32)

add_executable(${PROJECT_NAME} ${sources} )
target_link_libraries (${PROJECT_NAME} LINK_PUBLIC glfw Vulkan::Vulkan)
#set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")

#set_source_files_properties(${Freesync2_Shaders_src} PROPERTIES VS_TOOL_OVERRIDE "Text")

#function(copyCommand list dest)
#	foreach(fullFileName ${list})
#		get_filename_component(file ${fullFileName} NAME)
#		message("Generating custom command for ${fullFileName}")
#		add_custom_command(
#			OUTPUT   ${dest}/${file}
#			PRE_BUILD
#			COMMAND cmake -E make_directory ${dest}
#			COMMAND cmake -E copy ${fullFileName}  ${dest}
#			MAIN_DEPENDENCY  ${fullFileName}
#			COMMENT "Updating ${file} into ${dest}"
#		)
#	endforeach()
#endfunction()
#copyCommand("${Freesync2_Shaders_src}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibVK)