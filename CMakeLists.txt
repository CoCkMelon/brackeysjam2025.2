cmake_minimum_required(VERSION 3.21)
project(sidescroller_racer C CXX)

# Configure C/C++ standards (similar to ./trash/CMakeLists.txt)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer options
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer" OFF)

if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN OR ENABLE_MSAN)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        message(WARNING "Sanitizers work best with Debug or RelWithDebInfo build types")
    endif()
    set(SANITIZER_FLAGS "")
    if(ENABLE_ASAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=address")
        message(STATUS "AddressSanitizer enabled")
    endif()
    if(ENABLE_UBSAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
        message(STATUS "UndefinedBehaviorSanitizer enabled")
    endif()
    if(ENABLE_TSAN)
        if(ENABLE_ASAN)
            message(FATAL_ERROR "ThreadSanitizer cannot be used with AddressSanitizer")
        endif()
        list(APPEND SANITIZER_FLAGS "-fsanitize=thread")
        message(STATUS "ThreadSanitizer enabled")
    endif()
    if(ENABLE_MSAN)
        if(ENABLE_ASAN OR ENABLE_TSAN)
            message(FATAL_ERROR "MemorySanitizer cannot be used with AddressSanitizer or ThreadSanitizer")
        endif()
        list(APPEND SANITIZER_FLAGS "-fsanitize=memory" "-fsanitize-memory-track-origins")
        message(STATUS "MemorySanitizer enabled")
    endif()
    string(REPLACE ";" " " SANITIZER_FLAGS_STR "${SANITIZER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS_STR} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS_STR} -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS_STR}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_FLAGS_STR}")
endif()

# Engine repo options mirroring trash
set(AME_GIT_URL "https://github.com/CoCkMelon/A-Monogoose-Engine" CACHE STRING "Git URL for A-Monogoose-Engine")
set(AME_GIT_TAG "master" CACHE STRING "Git tag/branch for A-Monogoose-Engine")
add_compile_definitions(_GNU_SOURCE)

include(FetchContent)
# Ensure engine only builds what we actually use
set(AME_BUILD_EXAMPLES OFF CACHE BOOL "Disable engine examples" FORCE)
set(ASYNCINPUT_BUILD_EXAMPLES OFF CACHE BOOL "Disable asyncinput examples" FORCE)
# Hard-disable unitylike in the engine to avoid building unused targets
set(AME_BUILD_UNITYLIKE OFF CACHE BOOL "Disable building unitylike module" FORCE)
# Note: Do NOT force-disable flecs here; the engine's CMake install step expects flecs-export.
# We'll simply not link against flecs in this project.

set(AME_LOCAL_PATH "" CACHE PATH "Optional local path to A-Monogoose-Engine")
if(AME_LOCAL_PATH AND EXISTS "${AME_LOCAL_PATH}/CMakeLists.txt")
  message(STATUS "Using local A-Monogoose-Engine at ${AME_LOCAL_PATH}")
  set(ame_engine_SOURCE_DIR "${AME_LOCAL_PATH}")
  add_subdirectory("${AME_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/ame_engine_local")
else()
  FetchContent_Declare(
    ame_engine
    GIT_REPOSITORY ${AME_GIT_URL}
    GIT_TAG        ${AME_GIT_TAG}
  )
  FetchContent_MakeAvailable(ame_engine)
endif()

# Generate dialogues from YAML files
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/src/embedded_dialogues_gen.c ${CMAKE_SOURCE_DIR}/src/dialogue_generated.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/dialogue_yaml_to_c.py ${CMAKE_SOURCE_DIR}/dialogues ${CMAKE_SOURCE_DIR}/src
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/dialogue_yaml_to_c.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating dialogues from YAML files"
)

# Game executable (multi-file C with one C++ TU for tinyobjloader)
file(GLOB_RECURSE GAME_SOURCES
  CONFIGURE_DEPENDS
  src/*.c
  src/*.cpp
)
# Add generated dialogue files
list(APPEND GAME_SOURCES ${CMAKE_SOURCE_DIR}/src/embedded_dialogues_gen.c)
add_executable(game ${GAME_SOURCES})
set_property(TARGET game PROPERTY LINKER_LANGUAGE CXX)
# Keep SDL3 callbacks main style like example
target_compile_definitions(game PRIVATE SDL_MAIN_USE_CALLBACKS=1)

# Includes
target_include_directories(game PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/render
  ${CMAKE_SOURCE_DIR}/src/entities
  ${CMAKE_SOURCE_DIR}/src/obj
)


# Use tinyobjloader vendored with the engine
if(ame_engine_SOURCE_DIR)
  target_include_directories(game PRIVATE ${ame_engine_SOURCE_DIR}/third_party)
endif()

# Link with engine C API targets (avoid facade, but link libs it provides)
set(_ame_targets
  SDL3::SDL3
  Threads::Threads
  ame
  SDL3_image::SDL3_image
  SDL3_ttf::SDL3_ttf
  glad
  box2d
)
foreach(tgt IN LISTS _ame_targets)
  if(TARGET ${tgt})
    target_link_libraries(game PRIVATE ${tgt})
    message(STATUS "Linking game against: ${tgt}")
  endif()
endforeach()

# Also try to link against SDL3_ttf for systems where SDL_ttf is provided as SDL3
# Prefer pkg-config if available to pick up proper include and library paths
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL3TTF QUIET sdl3_ttf)
  if(SDL3TTF_FOUND)
    message(STATUS "Found SDL3_ttf via pkg-config: ${SDL3TTF_VERSION}")
    target_include_directories(game PRIVATE ${SDL3TTF_INCLUDE_DIRS})
    if(SDL3TTF_LIBRARY_DIRS)
      target_link_directories(game PRIVATE ${SDL3TTF_LIBRARY_DIRS})
    endif()
    target_link_libraries(game PRIVATE ${SDL3TTF_LIBRARIES})
  else()
    # Fallback to linking by name; many distros provide -lSDL3_ttf
    message(STATUS "pkg-config sdl3_ttf not found; linking SDL3_ttf by name")
    target_link_libraries(game PRIVATE SDL3_ttf)
  endif()
else()
  # If pkg-config isn't available, try a simple link by name
  message(STATUS "PkgConfig not found; linking SDL3_ttf by name")
  target_link_libraries(game PRIVATE SDL3_ttf)
endif()

# Avoid accidental linkage against unitylike/flecs if present in the superproject
if(TARGET unitylike)
  message(STATUS "Not linking unitylike (explicitly disabled)")
endif()
if(TARGET flecs)
  message(STATUS "Not linking flecs (explicitly disabled)")
endif()

# Ensure _GNU_SOURCE on imported targets
foreach(tgt IN LISTS _ame_targets)
  if(TARGET ${tgt})
    target_compile_definitions(${tgt} PRIVATE _GNU_SOURCE)
  endif()
endforeach()

# If asyncinput is available, include and link
# Prefer include dir exported by the engine if present, otherwise fall back to FetchContent path
if(DEFINED asyncinput_SOURCE_DIR AND EXISTS "${asyncinput_SOURCE_DIR}/include")
  target_include_directories(game PRIVATE "${asyncinput_SOURCE_DIR}/include")
elseif(EXISTS "${CMAKE_BINARY_DIR}/_deps/asyncinput-src/include")
  target_include_directories(game PRIVATE "${CMAKE_BINARY_DIR}/_deps/asyncinput-src/include")
endif()
if(TARGET asyncinput_shared)
  target_link_libraries(game PRIVATE asyncinput_shared)
  target_compile_definitions(asyncinput_shared PRIVATE _GNU_SOURCE)
elseif(TARGET asyncinput_static)
  target_link_libraries(game PRIVATE asyncinput_static)
  target_compile_definitions(asyncinput_static PRIVATE _GNU_SOURCE)
endif()

# Install
install(TARGETS game RUNTIME DESTINATION bin)

