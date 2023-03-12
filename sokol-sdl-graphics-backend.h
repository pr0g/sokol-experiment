#ifndef SOKOL_SDL_GRAPHICS_BACKEND_H
#define SOKOL_SDL_GRAPHICS_BACKEND_H

#include <stdbool.h>

typedef struct SDL_Window SDL_Window;
typedef struct sg_desc sg_desc;
typedef struct as_mat44f as_mat44f;

bool se_init_backend(SDL_Window* window);
sg_desc se_create_desc();
void se_init_imgui(SDL_Window* window);
as_mat44f se_perspective_projection(
  float aspect_ratio, float vertical_fov_radians, float near_plane,
  float far_plane);
as_mat44f se_orthographic_projection(
  float left, float right, float bottom, float top, float near_plane,
  float far_plane);
void se_present(SDL_Window* window);
void se_deinit_backend();

#endif // SOKOL_SDL_GRAPHICS_BACKEND_H