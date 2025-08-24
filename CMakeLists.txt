cmake_minimum_required(VERSION 3.21)
project(brackeysjam2025_2 CXX)

# Configure C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# The engine pulls in C++ sources (Box2D, physics); enable C++ and link with a C++ linker
enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optionally allow the user to override the engine repo/tag
set(AME_GIT_URL "https://github.com/CoCkMelon/A-Monogoose-Engine" CACHE STRING "Git URL for A-Monogoose-Engine")
set(AME_GIT_TAG "master" CACHE STRING "Git tag/branch for A-Monogoose-Engine")

# Enable feature-test macros for C sources globally (glibc: M_PI, strnlen, clock_gettime)
add_compile_definitions(_GNU_SOURCE)

# Fetch or use local engine
include(FetchContent)
# Ensure examples in engine and asyncinput are disabled in all cases
set(AME_BUILD_EXAMPLES OFF CACHE BOOL "Disable engine examples" FORCE)
set(ASYNCINPUT_BUILD_EXAMPLES OFF CACHE BOOL "Disable asyncinput examples" FORCE)

# Allow overriding with a local engine path to pick up local edits
set(AME_LOCAL_PATH "" CACHE PATH "Optional local path to A-Monogoose-Engine")
if(AME_LOCAL_PATH AND EXISTS "${AME_LOCAL_PATH}/CMakeLists.txt")
  message(STATUS "Using local A-Monogoose-Engine at ${AME_LOCAL_PATH}")
  # Mirror FetchContent variables so downstream logic can find headers
  set(ame_engine_SOURCE_DIR "${AME_LOCAL_PATH}")
  add_subdirectory("${AME_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/ame_engine_local")
else()
  FetchContent_Declare(
    ame_engine
    GIT_REPOSITORY ${AME_GIT_URL}
    GIT_TAG        ${AME_GIT_TAG}
  )
  # Make the engine available (adds the engine's CMake project via add_subdirectory)
  FetchContent_MakeAvailable(ame_engine)
endif()

# Your game executable
add_executable(game src/main.cpp src/input_local.cpp)
# Link using C++ linker since engine brings C++ objects
set_property(TARGET game PROPERTY LINKER_LANGUAGE CXX)
# Many examples use SDL3 callbacks style
target_compile_definitions(game PRIVATE SDL_MAIN_USE_CALLBACKS=1)

# Try to link against a known engine target; adjust this list if needed
set(_ame_targets
  SDL3::SDL3
  Threads::Threads
  unitylike
  ame
  flecs
  SDL3_image::SDL3_image
  glad
)
set(_ame_found FALSE)
foreach(tgt IN LISTS _ame_targets)
  if(TARGET ${tgt})
    target_link_libraries(game PRIVATE ${tgt})
    set(_ame_found TRUE)
    message(STATUS "Linking game against engine target: ${tgt}")
    break()
  endif()
endforeach()

# Ensure GNU extensions for engine target(s) to expose M_PI/strnlen on glibc
foreach(tgt IN LISTS _ame_targets)
  if(TARGET ${tgt})
    target_compile_definitions(${tgt} PRIVATE _GNU_SOURCE)
  endif()
endforeach()

# If asyncinput was fetched by the engine, hook it up for our game too
# Include headers for asyncinput so we can #include "asyncinput.h"
target_include_directories(game PRIVATE "${CMAKE_BINARY_DIR}/_deps/asyncinput-src/include")
# Link against the shared library if present so usage requirements propagate
if(TARGET asyncinput_shared)
  target_link_libraries(game PRIVATE asyncinput_shared)
  # Ensure GNU extensions for asyncinput impl on glibc (clock_gettime, O_CLOEXEC, usleep)
  target_compile_definitions(asyncinput_shared PRIVATE _GNU_SOURCE)
endif()
if(TARGET asyncinput_static)
  target_compile_definitions(asyncinput_static PRIVATE _GNU_SOURCE)
endif()

# Fallback: at least include engine headers if a target wasn't found
if(NOT _ame_found)
  if(DEFINED ame_engine_SOURCE_DIR AND EXISTS "${ame_engine_SOURCE_DIR}/include")
    target_include_directories(game PRIVATE "${ame_engine_SOURCE_DIR}/include")
    message(WARNING "A-Monogoose-Engine CMake target not found. Included headers from ${ame_engine_SOURCE_DIR}/include. You may need to link libraries manually.")
  elseif(DEFINED ame_engine_SOURCE_DIR)
    message(WARNING "A-Monogoose-Engine fetched, but no include directory detected. Please adjust CMake to your engine layout.")
  else()
    message(WARNING "A-Monogoose-Engine not available via FetchContent. Check AME_GIT_URL/AME_GIT_TAG.")
  endif()
endif()

# Install rule (optional)
install(TARGETS game RUNTIME DESTINATION bin)

