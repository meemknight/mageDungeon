cmake_minimum_required(VERSION 3.1)

cmake_policy(SET CMP0069 NEW)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

project(fastNoiseSIMD)

message(STATUS "FastNoiseSIMD: EMSCRIPTEN='${EMSCRIPTEN}' CMAKE_SYSTEM_NAME='${CMAKE_SYSTEM_NAME}' UNIX='${UNIX}'")

# Detect Emscripten reliably (EMSCRIPTEN var is not always set in subprojects)
set(IS_EMSCRIPTEN FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(IS_EMSCRIPTEN TRUE)
endif()

# Collect all sources
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/FastNoiseSIMD/*.cpp"
)

# On WebAssembly, build scalar fallback only (exclude x86/ARM SIMD backends)
if(IS_EMSCRIPTEN)
  list(FILTER MY_SOURCES EXCLUDE REGEX ".*_(avx2|avx512|neon|sse2|sse41)\\.cpp$")
endif()

add_library(fastNoiseSIMD)
target_sources(fastNoiseSIMD PRIVATE ${MY_SOURCES})
target_include_directories(fastNoiseSIMD PUBLIC "FastNoiseSIMD")

# Only add x86 SSE flag on real native UNIX builds (not Emscripten)
if(UNIX AND NOT IS_EMSCRIPTEN)
  target_compile_options(fastNoiseSIMD PRIVATE -msse4.1)
endif()
