cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0135 NEW)

project(sokol-experiment LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
  sokol
  GIT_REPOSITORY https://github.com/pr0g/sokol.git
  GIT_TAG ca675e34f29667ceae522d58bb5b95cbc4db83f2)
FetchContent_Declare(
  SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.26.0/SDL2-2.26.0.tar.gz
  URL_HASH MD5=35bc58cfe41b8fb6c8e6646be26fa47e)
FetchContent_Declare(
  as-c-math
  GIT_REPOSITORY https://github.com/pr0g/as-c-math.git
  GIT_TAG 844c217213ae011b6a538f25a39167f78c180fd6)
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG b1051403cf81b984f9fc11cc5916adb5afd34766
  SOURCE_SUBDIR cmake)

FetchContent_MakeAvailable(glad SDL2 as-c-math sokol)

list(APPEND CMAKE_MODULE_PATH ${glad_SOURCE_DIR})

glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main
                                              as-c-math sokol glad_gl_core_33)
