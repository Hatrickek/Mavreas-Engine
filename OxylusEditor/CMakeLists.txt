set(PROJECT_NAME OxylusEditor)

# Source groups
file(GLOB src "src/*.h" "src/*.hpp" "src/*.cpp")
source_group("src" FILES ${src})
file(GLOB_RECURSE Panels "src/Panels/*.h" "src/Panels/*.hpp" "src/Panels/*.cpp" )
source_group("src/Panels" FILES ${Panels})
file(GLOB_RECURSE Utils "src/Utils/*.h" "src/Utils/*.hpp" "src/Utils/*.cpp" )
source_group("src/Utils" FILES ${Utils})

set(EDITOR_SOURCE_FILES ${src} ${Panels} ${Utils})

# Shaders
if(MSVC)
    file(GLOB_RECURSE Shaders "Resources/Shaders/*.hlsl" "Resources/Shaders/*.hlsli" 
                              "Resources/Shaders/*.glsl" "Resources/Shaders/*.frag" "Resources/Shaders/*.vert" "Resources/Shaders/*.comp")
    source_group("Shaders" FILES ${Shaders})

    set(ALL_FILES ${EDITOR_SOURCE_FILES} ${Shaders})
    set_source_files_properties(${Shaders} PROPERTIES VS_TOOL_OVERRIDE "None") # Don't compile shaders with VS
else() 
    set(ALL_FILES ${EDITOR_SOURCE_FILES})
endif()

# Target
add_executable(${PROJECT_NAME} ${ALL_FILES})

set(ROOT_NAMESPACE OxylusEditor)

# Target name
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_DEBUG   "OxylusEditor"
    TARGET_NAME_RELEASE "OxylusEditor"
    TARGET_NAME_Distribution    "OxylusEditor"
)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/GLFW/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/ImGuizmo"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/miniaudio"
)

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:"
        "OX_DEBUG;"
        "_DEBUG;"
    ">"
    "$<$<CONFIG:Release>:"
        "OX_RELEASE;"
        "NDEBUG;"
    ">"
    "$<$<CONFIG:Distribution>:"
        "OX_DISTRIBUTION;"
        "NDEBUG"
    ">"
)

# Compile and link options
if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/OxylusEditor")
    target_compile_options(${PROJECT_NAME} PRIVATE PRIVATE /std:c++20 /permissive-)
    target_link_options(${PROJECT_NAME} PRIVATE PRIVATE /DEBUG:FULL)
	set(CMAKE_SHARED_LINKER_FLAGS_DISTRIBUTION ${CMAKE_SHARED_LINKER_FLAGS})
    set(CMAKE_EXE_LINKER_FLAGS_DISTRIBUTION ${CMAKE_EXE_LINKER_FLAGS})
endif()

if (MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_definitions("/MP")
endif()

# Link with oxylus.
target_link_libraries(${PROJECT_NAME} PRIVATE
    Oxylus
)

# Copy resource folder
add_custom_command(TARGET OxylusEditor POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/OxylusEditor/Resources $<TARGET_FILE_DIR:OxylusEditor>/Resources)
