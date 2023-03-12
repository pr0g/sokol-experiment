#include "sokol-sdl-graphics-backend.h"

#define SOKOL_GLCORE33
#define SOKOL_NO_DEPRECATED
#include <sokol_gfx.h>

#include <SDL.h>
#include <as-ops.h>
#include <glad/gl.h>

SDL_GLContext* g_context = NULL;

bool se_init_backend(SDL_Window* window) {
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  g_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, g_context);
  SDL_GL_SetSwapInterval(1); // enable vsync

  const int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
  if (version == 0) {
    printf("Failed to initialize OpenGL context\n");
    return false;
  }

  return true;
}

sg_desc se_create_desc() {
  return (sg_desc){0};
}

void se_init_imgui(SDL_Window* window) {
  ImGui_ImplSDL2_InitForOpenGL(window, g_context);
}

as_mat44f se_perspective_projection(
  float aspect_ratio, float vertical_fov_radians, float near_plane,
  float far_plane) {
  return as_mat44f_perspective_projection_depth_minus_one_to_one_lh(
    aspect_ratio, vertical_fov_radians, near_plane, far_plane);
}

as_mat44f se_orthographic_projection(
  float left, float right, float bottom, float top, float near_plane,
  float far_plane) {
  return as_mat44f_orthographic_projection_depth_minus_one_to_one_lh(
    left, right, bottom, top, near_plane, far_plane);
}

void se_present(SDL_Window* window) {
    SDL_GL_SwapWindow(window);
}

void se_deinit_backend() {
    
}