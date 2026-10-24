cmake_minimum_required(VERSION 3.20)
project(ControlToolkit LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Dear ImGui - exact version used by this project.
FetchContent_Declare(
  imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.5-docking.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

# MinHook - pulled automatically at configure time.
FetchContent_Declare(
  minhook_src
  URL https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.3.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(minhook_src)
if(NOT minhook_src_POPULATED)
  FetchContent_Populate(minhook_src)
endif()

set(MINHOOK_SOURCES
  ${minhook_src_SOURCE_DIR}/src/buffer.c
  ${minhook_src_SOURCE_DIR}/src/hook.c
  ${minhook_src_SOURCE_DIR}/src/trampoline.c
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  list(APPEND MINHOOK_SOURCES ${minhook_src_SOURCE_DIR}/src/hde/hde64.c)
else()
  list(APPEND MINHOOK_SOURCES ${minhook_src_SOURCE_DIR}/src/hde/hde32.c)
endif()

add_library(ControlToolkit SHARED
  src/dllmain.cpp
  src/toolkit.cpp
  src/toolkit.h
  src/northlight_lights.cpp
  src/northlight_lights.h

  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp

  ${MINHOOK_SOURCES}
)

target_include_directories(ControlToolkit PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${minhook_src_SOURCE_DIR}/include
  ${minhook_src_SOURCE_DIR}/src
  ${minhook_src_SOURCE_DIR}/src/hde
)

target_compile_definitions(ControlToolkit PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

if(MSVC)
  target_compile_options(ControlToolkit PRIVATE /EHsc)
endif()

target_link_libraries(ControlToolkit PRIVATE
  d3d12
  dxgi
  d3dcompiler
  user32
  gdi32
  shell32
)

set_target_properties(ControlToolkit PROPERTIES
  OUTPUT_NAME "ControlToolkit"
)

# Standalone x64 launcher, built alongside the DLL.
add_executable(ControlToolkitInjector injector/main.cpp)
target_compile_definitions(ControlToolkitInjector PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(ControlToolkitInjector PRIVATE kernel32)
