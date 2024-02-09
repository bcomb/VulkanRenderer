project(Simple)

# Add source to this project's executable.
add_executable(${PROJECT_NAME}  main.cpp App.h App.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")

target_link_libraries(${PROJECT_NAME} PUBLIC VulkanCore)


# add_custom_command(TARGET chapter_2 POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:chapter_2> $<TARGET_FILE_DIR:chapter_2>
#   COMMAND_EXPAND_LISTS
#   )