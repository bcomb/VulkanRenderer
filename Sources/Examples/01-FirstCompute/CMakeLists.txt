project(01-FirstCompute)

# Add source to this project's executable.
add_executable(${PROJECT_NAME}  main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")

target_link_libraries(${PROJECT_NAME} PUBLIC VulkanCore imgui)
