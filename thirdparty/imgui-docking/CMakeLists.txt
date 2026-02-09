cmake_minimum_required(VERSION 3.1)
project(imgui)

add_library(imgui)

# We keep SDLRenderer3 backend for legacy path and optionally build SDLGPU3
# so runtime can switch renderer backend based on gl2d mode.
set(IMGUI_BACKEND_SOURCES
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_sdl3.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_sdlrenderer3.cpp"
)

if(GL2D_USE_SDL_GPU)
  list(APPEND IMGUI_BACKEND_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_sdlgpu3.cpp"
  )
endif()

target_sources(imgui PRIVATE 
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
${IMGUI_BACKEND_SOURCES}
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/IconsForkAwesome.h"

)

target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui")


target_link_libraries(imgui PUBLIC SDL3::SDL3 )
