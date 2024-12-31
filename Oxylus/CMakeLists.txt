set(PROJECT_NAME Oxylus)

file(GLOB src "src/*.h" "src/*.hpp" "src/*.cpp")
source_group("src" FILES ${src})
file(GLOB_RECURSE Assets "src/Assets/*.h" "src/Assets/*.hpp" "src/Assets/*.cpp" )
source_group("src/Assets" FILES ${Assets})
file(GLOB_RECURSE Audio "src/Audio/*.h" "src/Audio/*.hpp" "src/Audio/*.cpp" )
source_group("src/Audio" FILES ${Audio})
file(GLOB_RECURSE Core "src/Core/*.h" "src/Core/*.hpp" "src/Core/*.cpp" )
source_group("src/Core" FILES ${Core})
file(GLOB_RECURSE Event "src/Event/*.h" "src/Event/*.hpp" "src/Event/*.cpp" )
source_group("src/Event" FILES ${Event})
file(GLOB_RECURSE Physics "src/Physics/*.h" "src/Physics/*.hpp" "src/Physics/*.cpp" )
source_group("src/Physics" FILES ${Physics})
file(GLOB_RECURSE Scripting "src/Scripting/*.h" "src/Scripting/*.hpp" "src/Scripting/*.cpp" )
source_group("src/Scripting" FILES ${Scripting})
file(GLOB_RECURSE Modules "src/Modules/*.h" "src/Modules/*.hpp" "src/Modules/*.cpp" )
source_group("src/Modules" FILES ${Modules})

file(GLOB Render "src/Render/*.h" "src/Render/*.hpp" "src/Render/*.cpp" )
source_group("src/Render" FILES ${Render})
file(GLOB Passes "src/Render/Passes/*.h" "src/Render/Passes/*.hpp" "src/Render/Passes/*.cpp" )
source_group("src/Render/Passes" FILES ${Passes})
file(GLOB Vulkan "src/Render/Vulkan/*.h" "src/Render/Vulkan/*.hpp" "src/Render/Vulkan/*.cpp" )
source_group("src/Render/Vulkan" FILES ${Vulkan})
file(GLOB RenderUtils "src/Render/Utils/*.h" "src/Render/Utils/*.hpp" "src/Render/Utils/*.cpp" )
source_group("src/Render/Utils" FILES ${RenderUtils})

set(RENDERER_FILES ${Render} ${Passes} ${Vulkan} ${RenderUtils})

file(GLOB_RECURSE Scene "src/Scene/*.h" "src/Scene/*.hpp" "src/Scene/*.cpp" )
source_group("src/Scene" FILES ${Scene})
file(GLOB_RECURSE Thread "src/Thread/*.h" "src/Thread/*.hpp" "src/Thread/*.cpp" )
source_group("src/Thread" FILES ${Thread})
file(GLOB_RECURSE UI "src/UI/*.h" "src/UI/*.hpp" "src/UI/*.cpp" )
source_group("src/UI" FILES ${UI})
file(GLOB_RECURSE Utils "src/Utils/*.h" "src/Utils/*.hpp" "src/Utils/*.cpp" )
source_group("src/Utils" FILES ${Utils})

set(SOURCE_FILES ${src} ${Core} ${Assets} ${Audio} ${Event} ${Physics} ${Modules}
                  ${RENDERER_FILES} ${Scene} ${Thread} ${UI} ${Utils} ${Scripting})

set(vendor_ImGuizmo "vendor/ImGuizmo/ImGuizmo.cpp" "vendor/ImGuizmo/ImGuizmo.h")
source_group("vendor/ImGuizmo" FILES ${vendor_ImGuizmo})

set(vendor_miniaudio "vendor/miniaudio/miniaudio.h")
source_group("vendor/miniaudio" FILES ${vendor_miniaudio})

set(vendor_stb "vendor/stb/stb_image.h" "vendor/stb/stb_image.cpp")
source_group("vendor/stb" FILES ${vendor_stb})

set(ALL_FILES
  ${SOURCE_FILES}
  ${vendor_ImGuizmo}
  ${vendor_miniaudio}
  ${vendor_stb}
)

# Target
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb"
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
  "_CRT_SECURE_NO_WARNINGS"
  "GLFW_INCLUDE_NONE"
  "_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS"
  "GLM_FORCE_DEPTH_ZERO_TO_ONE"
  "GLM_ENABLE_EXPERIMENTAL"
  )

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_definitions(${PROJECT_NAME} PRIVATE "OX_COMPILER_CLANG")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_definitions(${PROJECT_NAME} PRIVATE "OX_COMPILER_GCC")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    target_compile_definitions(${PROJECT_NAME} PRIVATE "OX_COMPILER_INTEL")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_definitions(${PROJECT_NAME} PRIVATE "OX_COMPILER_MSVC")
endif()

#-------------
# MSVC Specific
#-------------

# Compile and link options
if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /std:c++20 /permissive- /W3 /EHsc) 
endif ()

if (MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_definitions("/MP")
  add_definitions("/bigobj")
endif()

#-------------
# Dependencies
#-------------
if (NOT EXISTS ${CMAKE_SOURCE_DIR}/cpm.cmake)
  file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.7/CPM.cmake ${CMAKE_SOURCE_DIR}/cpm.cmake SHOW_PROGRESS)
endif()

include(${CMAKE_SOURCE_DIR}/cpm.cmake)
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1) # hack for removing auto generated ctest targets

include(FetchContent)

# Vulkan
if (VULKAN_SDK)
    message("Within Oxylus CMake, setting VULKAN_SDK env var to " ${VULKAN_SDK})
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})

# dear imgui
CPMAddPackage("gh:ocornut/imgui#docking")
target_include_directories(${PROJECT_NAME} PUBLIC ${imgui_SOURCE_DIR})

file(GLOB ImGuiFiles "${imgui_SOURCE_DIR}/*.h" "${imgui_SOURCE_DIR}/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE "${ImGuiFiles}")
source_group("vendor/ImGui" FILES ${ImGuiFiles})

set(ImGui_Backends
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.h"
)
target_sources(${PROJECT_NAME} PRIVATE "${ImGui_Backends}")
source_group("vendor/ImGui" FILES ${ImGui_Backends})

set(ImGui_Misc "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp" "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h")
target_sources(${PROJECT_NAME} PRIVATE "${ImGui_Misc}")
source_group("vendor/ImGui" FILES ${ImGui_Misc})

#glm 
CPMAddPackage(
  NAME glm
  GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e #refs/tags/1.0.1
  GITHUB_REPOSITORY g-truc/glm
  GIT_SHALLOW ON
)
target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm)

# entt
CPMAddPackage(
  NAME EnTT
  VERSION 3.12.2
  GITHUB_REPOSITORY skypjack/entt
  DOWNLOAD_ONLY YES
  GIT_SHALLOW ON
  OPTIONS
	"ENTT_INCLUDE_NATVIS ON"
)
if (EnTT_ADDED)
    add_library(EnTT INTERFACE)
    target_include_directories(EnTT INTERFACE ${EnTT_SOURCE_DIR}/src)
    target_link_libraries(${PROJECT_NAME} PUBLIC EnTT)
endif()

# fastgltf
CPMAddPackage(
    NAME fastgltf
    VERSION 0.8.0
    GITHUB_REPOSITORY spnda/fastgltf
    GIT_SHALLOW ON
    OPTIONS
        "FASTGLTF_COMPILE_AS_CPP20 ON"
        "FASTGLTF_USE_CUSTOM_SMALLVECTOR ON"
)
set_target_properties(fastgltf PROPERTIES FOLDER "Vendor")
target_link_libraries(${PROJECT_NAME} PUBLIC fastgltf)

# meshoptimizer
CPMAddPackage("gh:zeux/meshoptimizer#v0.20")
target_link_libraries(${PROJECT_NAME} PUBLIC meshoptimizer)
set_target_properties(meshoptimizer PROPERTIES FOLDER "Vendor")

# FMT
CPMAddPackage("gh:fmtlib/fmt#10.1.1")
target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt)
set_target_properties(fmt PROPERTIES FOLDER "Vendor")

# fmtlog
CPMAddPackage(
    NAME loguru
    GITHUB_REPOSITORY emilk/loguru
    GIT_TAG master
    GIT_SHALLOW ON
    OPTIONS
        "LOGURU_USE_FMTLIB ON"
        "LOGURU_BUILD_EXAMPLES ON"
        "LOGURU_BUILD_TESTS ON"
)
target_link_libraries(${PROJECT_NAME} PUBLIC loguru)
set_target_properties(loguru PROPERTIES FOLDER "Vendor")

# vk-bootstrap
CPMAddPackage("gh:charles-lunarg/vk-bootstrap#a5950c070082e851e621206fca7b3b0b008dcb28")
target_link_libraries(${PROJECT_NAME} PUBLIC vk-bootstrap)
set_target_properties(vk-bootstrap PROPERTIES FOLDER "Vendor")

# vuk
CPMAddPackage(
    NAME vuk
    GITHUB_REPOSITORY martty/vuk
    GIT_TAG new-FE
    GIT_SHALLOW ON
    OPTIONS
        "VUK_LINK_TO_LOADER OFF"
        "VUK_FAIL_FAST ON"
        "VUK_USE_DXC ON"
)
target_compile_definitions(vuk PUBLIC VUK_MAX_VIEWPORTS=16 VUK_MAX_SCISSORS=16)
target_link_libraries(${PROJECT_NAME} PUBLIC vuk)
target_include_directories(${PROJECT_NAME} PUBLIC ${vuk_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PUBLIC ${vuk_SOURCE_DIR}/include)
set_target_properties(vuk PROPERTIES FOLDER "Vendor")
set_target_properties(spirv-cross-core PROPERTIES FOLDER "Vendor")
set_target_properties(spirv-cross-glsl PROPERTIES FOLDER "Vendor")

# NFD
CPMAddPackage("gh:btzy/nativefiledialog-extended#master")
target_link_libraries(${PROJECT_NAME} PUBLIC nfd)
target_include_directories(${PROJECT_NAME} PUBLIC ${NFD_SOURCE_DIR}/src/include)
set_target_properties(nfd PROPERTIES FOLDER "Vendor")

# GLFW
CPMAddPackage(
    NAME GLFW
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.4
    GIT_SHALLOW ON
    OPTIONS
    "GLFW_BUILD_TESTS OFF"
    "GLFW_BUILD_EXAMPLES OFF"
    "GLFW_BULID_DOCS OFF"
	"GLFW_INSTALL OFF"
)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
set_target_properties(glfw PROPERTIES FOLDER "Vendor")
set_target_properties(update_mappings PROPERTIES FOLDER "Vendor")

# toml
CPMAddPackage("gh:marzer/tomlplusplus#master")
target_include_directories(${PROJECT_NAME} PUBLIC ${tomlplusplus_SOURCE_DIR}/include)

# RapidJSON
CPMAddPackage(
    NAME rapidjson
    GITHUB_REPOSITORY Tencent/rapidjson
    GIT_TAG master
    DOWNLOAD_ONLY YES
    GIT_SHALLOW ON
)
target_include_directories(${PROJECT_NAME} PUBLIC ${rapidjson_SOURCE_DIR}/include)

# Jolt
CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    VERSION 5.0.0 
    SOURCE_SUBDIR Build
    GIT_SHALLOW ON
    OPTIONS
        "DISABLE_CUSTOM_ALLOCATOR ON"
        "ENABLE_ALL_WARNINGS OFF"
        "USE_STATIC_MSVC_RUNTIME_LIBRARY OFF"
        "INTERPROCEDURAL_OPTIMIZATION OFF"
)
target_link_libraries(${PROJECT_NAME} PUBLIC Jolt)
target_include_directories(${PROJECT_NAME} PUBLIC ${JoltPhysics_SOURCE_DIR}/..)
set_target_properties(Jolt PROPERTIES FOLDER "Vendor")

# tracy
CPMAddPackage(
    NAME TracyClient
    GITHUB_REPOSITORY wolfpld/tracy
    VERSION 0.10
    GIT_SHALLOW ON
    OPTIONS
        "TRACY_ENABLE OFF"
        "TRACY_STATIC ON"
        "TRACY_VK_USE_SYMBOL_TABLE ON"
        "TRACY_NO_FRAME_IMAGE ON"
        "TRACY_ON_DEMAND ON"
)
target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
set_target_properties(TracyClient PROPERTIES FOLDER "Vendor")

CPMAddPackage(
    NAME lua
    GITHUB_REPOSITORY walterschell/Lua
    GIT_TAG 88246d621abf7b6fba9332f49229d507f020e450
    GIT_SHALLOW ON
    OPTIONS
        "LUA_SUPPORT_DL OFF"
        "LUA_BUILD_AS_CXX OFF"
        "LUA_ENABLE_SHARED OFF"
        "LUA_ENABLE_TESTING OFF"
        "LUA_BUILD_COMPILER OFF"
)
target_include_directories(${PROJECT_NAME} PUBLIC ${lua_SOURCE_DIR}/lua-5.4.6/include)

CPMAddPackage(
    NAME sol2
    GITHUB_REPOSITORY Hatrickek/sol2
    GIT_TAG develop
    OPTIONS
        "SOL2_BUILD_LUA OFF"
        "SOL2_TESTS OFF"
        "SOL2_TESTS_SINGLE OFF"
)
target_include_directories(${PROJECT_NAME} PUBLIC ${sol2_SOURCE_DIR}/include)
target_link_libraries(sol2 INTERFACE $<BUILD_INTERFACE:${lua_static}>)
target_link_libraries(${PROJECT_NAME} PUBLIC sol2 lua_static)
set_target_properties(lua_static PROPERTIES FOLDER "Vendor")

# enkiTS
CPMAddPackage(
    NAME enkiTS
    GITHUB_REPOSITORY dougbinks/enkiTS
    GIT_TAG master
    GIT_SHALLOW ON
    OPTIONS
        "ENKITS_BUILD_C_INTERFACE OFF"
        "ENKITS_BUILD_EXAMPLES OFF"
)
target_include_directories(${PROJECT_NAME} PUBLIC ${enkiTS_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} PUBLIC enkiTS)
set_target_properties(enkiTS PROPERTIES FOLDER "Vendor")

# unordered_dense
CPMAddPackage("gh:martinus/unordered_dense#main")
target_link_libraries(${PROJECT_NAME} PUBLIC unordered_dense)
set_target_properties(unordered_dense PROPERTIES FOLDER "Vendor")

# dylib
CPMAddPackage("gh:martin-olivier/dylib#main")
target_include_directories(${PROJECT_NAME} PUBLIC ${dylib_SOURCE_DIR}/include)

# ktx
CPMAddPackage(
    NAME ktx
    GITHUB_REPOSITORY KhronosGroup/KTX-Software
    GIT_TAG main
    GIT_SHALLOW ON
    GIT_MODULES ""
    OPTIONS
        "KTX_FEATURE_STATIC_LIBRARY ON"
        "KTX_FEATURE_LOADTEST_APPS OFF"
        "KTX_FEATURE_DOC OFF"
        "KTX_FEATURE_TESTS OFF"
        "KTX_FEATURE_GL_UPLOAD OFF"
        "KTX_FEATURE_TOOLS_CTS OFF"
        "KTX_FEATURE_TOOLS OFF"
)
target_include_directories(${PROJECT_NAME} PUBLIC ${ktx_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC ktx)
set_target_properties(ktx PROPERTIES FOLDER "Vendor")
set_target_properties(astcenc-avx2-static PROPERTIES FOLDER "Vendor")
set_target_properties(ktx_read PROPERTIES FOLDER "Vendor")
set_target_properties(ktx_version PROPERTIES FOLDER "Vendor")
set_target_properties(obj_basisu_cbind PROPERTIES FOLDER "Vendor")
set_target_properties(objUtil PROPERTIES FOLDER "Vendor")

# imspinner
CPMAddPackage("gh:dalerank/imspinner#254b46994a76222a6da06111978a1674865a050d")
target_include_directories(${PROJECT_NAME} PUBLIC ${imspinner_SOURCE_DIR})
