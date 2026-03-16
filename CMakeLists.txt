#https://github.com/meemknight/SDL3NoglCMakeSetup
#Version 1.0.0

cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

#! ! ! ! ! ! !
#set this to true to ship the game!
#basically this will change the RESOURCES_PATH to be the local path
#! ! ! ! ! ! !
# These are forced in cache so manual ON/OFF edits in this file always apply.
set(PRODUCTION_BUILD OFF CACHE BOOL "Make this a production build" FORCE)

# Enables the new SDL_gpu batching path in gl2d.
# Web builds currently fall back to the legacy SDL renderer path because the
# gameplay custom pipelines/shaders are authored for Vulkan/SPIR-V only.
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(GL2D_USE_SDL_GPU OFF CACHE BOOL "Use SDL_gpu backend for gl2d" FORCE)
else()
  set(GL2D_USE_SDL_GPU ON CACHE BOOL "Use SDL_gpu backend for gl2d" FORCE)
endif()
if(GL2D_USE_SDL_GPU)
  set(GL2D_USE_SDL_GPU_VALUE 1)
else()
  set(GL2D_USE_SDL_GPU_VALUE 0)
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) #link time optimization

if(MSVC)
  add_compile_options(/arch:AVX2) #make sure SIMD optimizations take place
endif()

project(mageDunegon)

# Prefer static SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/SDL-release-3.2.28)     # window opener (SDL3)
add_subdirectory(thirdparty/stb_image)              # loading images
add_subdirectory(thirdparty/stb_truetype)           # loading ttf files
add_subdirectory(thirdparty/glm)                    # math
add_subdirectory(thirdparty/imgui-docking)          # ui
# add_subdirectory(thirdparty/raudio)					#audio
add_subdirectory(thirdparty/magic_enum-0.9.3)
add_subdirectory(thirdparty/FastNoiseSIMD)

# MY_SOURCES is defined to be a list of all the source files for my game
# DON'T ADD THE SOURCES BY HAND, they are already added with this macro
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable("${CMAKE_PROJECT_NAME}")
set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 17)

target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC GLFW_INCLUDE_NONE=1)
target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC GL2D_USE_SDL_GPU=${GL2D_USE_SDL_GPU_VALUE})
target_compile_definitions(imgui PUBLIC GL2D_USE_SDL_GPU=${GL2D_USE_SDL_GPU_VALUE})


if(EMSCRIPTEN)
  target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="/resources/")
elseif(PRODUCTION_BUILD)
  target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="./resources/")
else()
  target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
endif()


if(PRODUCTION_BUILD)
  # setup the ASSETS_PATH macro to be in the root folder of your exe
  target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=1)
  target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC DEVELOPLEMT_BUILD=0)
else()
  # This is useful to get an ASSETS_PATH in your IDE during development
  target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=0)
  target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC DEVELOPLEMT_BUILD=1)
endif()

target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES})

if(MSVC) # If using the VS compiler...
  target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)
  #add this line if you want to remove the console!
  #set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup") #no console
endif()

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/platform")

# SDL3 target name is typically SDL3::SDL3 for CMake usage (created by SDL's CMake)
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE
  glm
  stb_image
  stb_truetype
  imgui
  SDL3::SDL3
  # raudio
  magic_enum
  fastNoiseSIMD
)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES SUFFIX ".html")

  target_link_options("${CMAKE_PROJECT_NAME}" PRIVATE
	"-sALLOW_MEMORY_GROWTH=1"
	"--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html"
	"--preload-file" "${CMAKE_SOURCE_DIR}/resources@resources"
  )
endif()


# no warnings
if(EMSCRIPTEN)
  target_compile_options("${CMAKE_PROJECT_NAME}" PRIVATE
	-w
  )
endif()
