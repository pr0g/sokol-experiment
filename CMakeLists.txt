cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0135 NEW)

project(sokol-experiment LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
  sokol
  GIT_REPOSITORY https://github.com/pr0g/sokol.git
  GIT_TAG aa7713a1e2b228e2f012bd18d0e906bc6116b807)
FetchContent_Declare(
  SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.26.0/SDL2-2.26.0.tar.gz
  URL_HASH MD5=35bc58cfe41b8fb6c8e6646be26fa47e)
FetchContent_Declare(
  as-c-math
  GIT_REPOSITORY https://github.com/pr0g/as-c-math.git
  GIT_TAG 024955c7538d56272c696c4b2d0cdd681b9d0d50)
FetchContent_Declare(
  upng
  GIT_REPOSITORY https://github.com/pr0g/upng.git
  GIT_TAG edb284b15fc3c37260d27182e4cf3a366c1734bf)
FetchContent_Declare(
  cimgui
  GIT_REPOSITORY https://github.com/cimgui/cimgui.git
  GIT_TAG d159c2622d21d1e21ae6a5970d0a8391e0957496)

option(IMGUI_STATIC "" ON)
FetchContent_MakeAvailable(SDL2 as-c-math sokol upng cimgui)

add_executable(${PROJECT_NAME})
target_sources(
  ${PROJECT_NAME}
  PRIVATE main.c
          other/mesh.c
          other/triangle.c
          other/texture.c
          other/array.c
          other/camera.c
          other/frustum.c
          imgui/imgui_impl_sdl.c)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main
                                              as-c-math sokol upng cimgui)
target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE $<$<BOOL:${SOKOL_EXPERIMENT_GL}>:SOKOL_EXPERIMENT_GL>
          $<$<BOOL:${SOKOL_EXPERIMENT_D3D}>:SOKOL_EXPERIMENT_D3D>)

if(SOKOL_EXPERIMENT_GL)
  FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG b1051403cf81b984f9fc11cc5916adb5afd34766
    SOURCE_SUBDIR cmake)
  FetchContent_MakeAvailable(glad)
  list(APPEND CMAKE_MODULE_PATH ${glad_SOURCE_DIR})
  glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)
  target_link_libraries(${PROJECT_NAME} PRIVATE glad_gl_core_33)
  target_sources(${PROJECT_NAME} PRIVATE sokol-sdl-graphics-backend-gl.c)
elseif(SOKOL_EXPERIMENT_D3D)
  target_link_libraries(${PROJECT_NAME} PRIVATE dxguid.lib)
  target_sources(${PROJECT_NAME} PRIVATE sokol-sdl-graphics-backend-d3d.c)
endif()

if(WIN32)
  # copy the SDL2.dll to the same folder as the executable
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    VERBATIM)
endif()
