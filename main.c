#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_NO_DEPRECATED
#include <glad/gl.h>
#include <sokol_gfx.h>

#include "shader/line.h"
#include "shader/projected.h"
#include "shader/standard.h"

#include "imgui/imgui_impl_sdl.h"

#define SOKOL_IMGUI_NO_SOKOL_APP
#define SOKOL_IMGUI_IMPL
#include <util/sokol_imgui.h>

#include <SDL.h>
#include <as-ops.h>
#include <float.h>

#include "other/array.h"
#include "other/camera.h"
#include "other/frustum.h"
#include "other/mesh.h"

typedef enum movement_e {
  movement_up = 1 << 0,
  movement_down = 1 << 1,
  movement_left = 1 << 2,
  movement_right = 1 << 3,
  movement_forward = 1 << 4,
  movement_backward = 1 << 5
} movement_e;

// projection to use when in projected mode
typedef enum view_e { view_perspective, view_orthographic } view_e;
// mode of rendering
typedef enum mode_e { mode_standard, mode_projected } mode_e;

camera_t g_camera = {0};
int8_t g_movement = 0;
as_point2i g_mouse_position = {0};
bool g_mouse_down = false;
mode_e g_mode = mode_standard;
view_e g_view = view_orthographic;
as_mat34f g_model_transform = {0};
bool g_affine = false;

static void update_movement(const float delta_time) {
  const float speed = delta_time * 4.0f;
  if ((g_movement & movement_forward) != 0) {
    const as_mat33f rotation = camera_rotation(&g_camera);
    g_camera.pivot = as_point3f_add_vec3f(
      g_camera.pivot, as_mat33f_mul_vec3f(&rotation, (as_vec3f){.z = speed}));
  }
  if ((g_movement & movement_left) != 0) {
    const as_mat33f rotation = camera_rotation(&g_camera);
    g_camera.pivot = as_point3f_add_vec3f(
      g_camera.pivot, as_mat33f_mul_vec3f(&rotation, (as_vec3f){.x = -speed}));
  }
  if ((g_movement & movement_backward) != 0) {
    const as_mat33f rotation = camera_rotation(&g_camera);
    g_camera.pivot = as_point3f_add_vec3f(
      g_camera.pivot, as_mat33f_mul_vec3f(&rotation, (as_vec3f){.z = -speed}));
  }
  if ((g_movement & movement_right) != 0) {
    const as_mat33f rotation = camera_rotation(&g_camera);
    g_camera.pivot = as_point3f_add_vec3f(
      g_camera.pivot, as_mat33f_mul_vec3f(&rotation, (as_vec3f){.x = speed}));
  }
  if ((g_movement & movement_down) != 0) {
    g_camera.pivot =
      as_point3f_add_vec3f(g_camera.pivot, (as_vec3f){.y = -speed});
  }
  if ((g_movement & movement_up) != 0) {
    g_camera.pivot =
      as_point3f_add_vec3f(g_camera.pivot, (as_vec3f){.y = speed});
  }
}

int main(int argc, char** argv) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  const int width = 1024;
  const int height = 768;
  SDL_Window* window = SDL_CreateWindow(
    argv[0], SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height,
    SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);

  if (window == NULL) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GLContext* context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, context);
  SDL_GL_SetSwapInterval(1); // enable vsync

  const int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
  if (version == 0) {
    printf("Failed to initialize OpenGL context\n");
    return 1;
  }

  model_t model = load_obj_mesh_with_png_texture(
    "assets/models/f22.obj", "assets/textures/f22.png");

  // setup model data
  float* model_vertices = NULL;
  float* model_uvs = NULL;
  uint16_t* model_indices = NULL;
  uint16_t index = 0;
  const int face_count = array_length(model.mesh.faces);
  for (int f = 0; f < face_count; f++) {
    for (int v = 0; v < 3; v++) {
      const int vertex_index = model.mesh.faces[f].vert_indices[v] - 1;
      array_push(model_vertices, model.mesh.vertices[vertex_index].x);
      array_push(model_vertices, model.mesh.vertices[vertex_index].y);
      array_push(model_vertices, model.mesh.vertices[vertex_index].z);

      const int uv_index = model.mesh.faces[f].uv_indices[v] - 1;
      array_push(model_uvs, model.mesh.uvs[uv_index].u);
      array_push(model_uvs, 1.0f - model.mesh.uvs[uv_index].v);

      array_push(model_indices, index);
      index++;
    }
  }

  array_free(model.mesh.vertices);
  array_free(model.mesh.uvs);
  array_free(model.mesh.faces);

  // setup sokol_gfx
  sg_setup(&(sg_desc){0});
  simgui_setup(&(simgui_desc_t){.ini_filename = "imgui.ini"});
  ImGui_ImplSDL2_InitForOpenGL(window, context);

  g_model_transform = as_mat34f_translation_from_vec3f((as_vec3f){.z = 5.0f});

  float fov_degrees = 60.0f;
  float near_plane = 2.0f;
  float far_plane = 10.0f;

  const as_mat44f perspective_projection_projected_mode =
    as_mat44f_perspective_projection_depth_minus_one_to_one_lh(
      (float)width / (float)height, as_radians_from_degrees(60.0f), 0.01f,
      100.0f);

  float* projected_vertices = NULL;
  projected_vertices =
    array_hold(projected_vertices, array_length(model_vertices), sizeof(float));

  float* vertex_depth_recips = NULL;
  vertex_depth_recips = array_hold(
    vertex_depth_recips, array_length(model_vertices) / 3, sizeof(float));

  // clang-format off
  const int cube_line_indices_count = 24;
  const int axes_line_indices_count = 6;
  const float unit_lines[] = {// ndc cube
                              -1.0f, -1.0f, -1.0f, // near bottom left
                               1.0f, -1.0f, -1.0f, // near bottom right
                               1.0f,  1.0f, -1.0f, // near top right
                              -1.0f,  1.0f, -1.0f, // near top left
                              -1.0f, -1.0f,  1.0f, // far bottom left
                               1.0f, -1.0f,  1.0f, // far bottom right
                               1.0f,  1.0f,  1.0f, // far top right
                              -1.0f,  1.0f,  1.0f, // far top left
                              // axes
                              -100.0f,  0.0f,    0.0f,
                               100.0f,  0.0f,    0.0f,
                               0.0f,   -100.0f,  0.0f,
                               0.0f,    100.0f,  0.0f,
                               0.0f,    0.0f,   -100.0f,
                               0.0f,    0.0f,    100.0f  };
  const uint16_t line_indices[] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
                                   6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7,
                                   8, 9, 10, 11, 12, 13};
  const uint32_t line_colors[] = {0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffaaaaaa,
                                  0xffaaaaaa,
                                  0xffaaaaaa,
                                  0xffaaaaaa,
                                  0xffaaaaaa,
                                  0xffaaaaaa};
  // clang-format on

  float lines[sizeof(unit_lines) / sizeof(float)];
  memcpy(&lines, unit_lines, sizeof(lines));

  sg_buffer line_buffer =
    sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(lines)});
  sg_buffer line_color_buffer =
    sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(line_colors)});
  sg_buffer line_index_buffer = sg_make_buffer(&(sg_buffer_desc){
    .type = SG_BUFFERTYPE_INDEXBUFFER, .data = SG_RANGE(line_indices)});

  sg_buffer standard_vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){
      .ptr = model_vertices,
      .size = array_length(model_vertices) * sizeof(float)}});
  sg_buffer projected_vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){
      .ptr = projected_vertices,
      .size = array_length(projected_vertices) * sizeof(float)}});
  sg_buffer uv_buffer = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){
      .ptr = model_uvs, .size = array_length(model_uvs) * sizeof(float)}});
  sg_buffer vertex_depth_recip_buffer = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){
      .ptr = vertex_depth_recips,
      .size = array_length(vertex_depth_recips) * sizeof(float)}});
  sg_buffer index_buffer = sg_make_buffer(&(sg_buffer_desc){
    .type = SG_BUFFERTYPE_INDEXBUFFER,
    .data = (sg_range){
      .ptr = model_indices,
      .size = array_length(model_indices) * sizeof(uint16_t)}});

  typedef struct vs_params_t {
    as_mat44f mvp;
  } vs_params_t;

  const sg_shader shader_projected =
    sg_make_shader(projected_shader_desc(sg_query_backend()));
  const sg_shader shader_standard =
    sg_make_shader(standard_shader_desc(sg_query_backend()));
  const sg_shader shader_line =
    sg_make_shader(line_shader_desc(sg_query_backend()));

  const sg_pipeline_desc pip_projected_desc = (sg_pipeline_desc){
    .shader = shader_projected,
    .layout =
      {.attrs =
         {[0] = {.format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0},
          [1] = {.format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1},
          [2] = {.format = SG_VERTEXFORMAT_FLOAT, .buffer_index = 2}}},
    .index_type = SG_INDEXTYPE_UINT16,
    .depth =
      {
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true,
      },
    .cull_mode = SG_CULLMODE_BACK,
    .face_winding = SG_FACEWINDING_CW};
  const sg_pipeline pip_projected = sg_make_pipeline(&pip_projected_desc);

  const sg_pipeline_desc pip_projected_desc_affine = (sg_pipeline_desc){
    .shader = shader_standard,
    .layout = pip_projected_desc.layout,
    .index_type = pip_projected_desc.index_type,
    .depth = pip_projected_desc.depth,
    .cull_mode = pip_projected_desc.cull_mode,
    .face_winding = pip_projected_desc.face_winding};
  const sg_pipeline pip_projected_affine =
    sg_make_pipeline(&pip_projected_desc_affine);

  sg_pipeline pip_standard = sg_make_pipeline(&(sg_pipeline_desc){
    .shader = shader_standard,
    .layout =
      {.attrs =
         {[0] = {.format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0},
          [1] = {.format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1}}},
    .index_type = SG_INDEXTYPE_UINT16,
    .depth =
      {
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true,
      },
    .cull_mode = SG_CULLMODE_BACK,
    .face_winding = SG_FACEWINDING_CW});

  sg_pipeline pip_line = sg_make_pipeline(&(sg_pipeline_desc){
    .shader = shader_line,
    .layout =
      {.attrs =
         {[0] = {.format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0},
          [1] = {.format = SG_VERTEXFORMAT_UBYTE4N, .buffer_index = 1}}},
    .index_type = SG_INDEXTYPE_UINT16,
    .depth =
      {
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true,
      },
    .primitive_type = SG_PRIMITIVETYPE_LINES});

  sg_image model_image = sg_make_image(&(sg_image_desc){
    .width = model.texture.width,
    .height = model.texture.height,
    .data.subimage[0][0] =
      (sg_range){
        .ptr = model.texture.color_buffer,
        .size = model.texture.width * model.texture.height * sizeof(uint32_t)},
    .label = "model-texture"});

  // resource bindings
  sg_bindings bind_projected = {
    .vertex_buffers =
      {[0] = projected_vertex_buffer,
       [1] = uv_buffer,
       [2] = vertex_depth_recip_buffer},
    .vertex_buffer_offsets = {[0] = 0, [1] = 0, [2] = 0},
    .index_buffer = index_buffer,
    .fs_images[0] = model_image};

  sg_bindings bind_standard = {
    .vertex_buffers = {[0] = standard_vertex_buffer, [1] = uv_buffer},
    .vertex_buffer_offsets = {[0] = 0, [1] = 0},
    .index_buffer = index_buffer,
    .fs_images[0] = model_image};

  sg_bindings bind_line = {
    .vertex_buffers = {[0] = line_buffer, [1] = line_color_buffer},
    .vertex_buffer_offsets = {[0] = 0, [1] = 0},
    .index_buffer = line_index_buffer};

  // default pass action (clear to grey)
  sg_pass_action pass_action = {0};

  typedef struct pinned_camera_t {
    camera_t camera;
    float fov_degrees;
    float near_plane;
    float far_plane;
  } pinned_camera_t;

  camera_t projected_camera = {0};
  pinned_camera_t pinned_camera_state = {
    .camera = {0},
    .fov_degrees = fov_degrees,
    .far_plane = far_plane,
    .near_plane = near_plane};

  bool pin_camera = false;
  bool draw_axes = false;
  vs_params_t vs_params_model;
  vs_params_t vs_params_lines;
  uint64_t previous_counter = 0;
  for (bool quit = false; !quit;) {
    const uint64_t current_counter = SDL_GetPerformanceCounter();
    const double delta_time = (double)(current_counter - previous_counter)
                            / (double)SDL_GetPerformanceFrequency();
    previous_counter = current_counter;

    mode_e current_mode = g_mode;
    for (SDL_Event current_event; SDL_PollEvent(&current_event) != 0;) {
      ImGui_ImplSDL2_ProcessEvent(&current_event);
      if (igGetIO()->WantCaptureMouse) {
        continue;
      }
      switch (current_event.type) {
        case SDL_QUIT: {
          quit = true;
        } break;
        case SDL_MOUSEMOTION: {
          const SDL_MouseMotionEvent* mouse_motion_event =
            (const SDL_MouseMotionEvent*)&current_event;
          const as_point2i previous_mouse_position = g_mouse_position;
          g_mouse_position = (as_point2i){
            .x = mouse_motion_event->x, .y = mouse_motion_event->y};
          if (g_mouse_down) {
            const as_vec2i mouse_delta =
              as_point2i_sub_point2i(g_mouse_position, previous_mouse_position);
            g_camera.pitch += (float)mouse_delta.y * 0.005f;
            g_camera.yaw += (float)mouse_delta.x * 0.005f;
          }
        } break;
        case SDL_MOUSEBUTTONDOWN: {
          g_mouse_down = true;
        } break;
        case SDL_MOUSEBUTTONUP: {
          g_mouse_down = false;
        } break;
        case SDL_KEYDOWN: {
          if (current_event.key.keysym.sym == SDLK_ESCAPE) {
            return false;
          } else if (current_event.key.keysym.sym == SDLK_w) {
            g_movement |= movement_forward;
          } else if (current_event.key.keysym.sym == SDLK_a) {
            g_movement |= movement_left;
          } else if (current_event.key.keysym.sym == SDLK_s) {
            g_movement |= movement_backward;
          } else if (current_event.key.keysym.sym == SDLK_d) {
            g_movement |= movement_right;
          } else if (current_event.key.keysym.sym == SDLK_q) {
            g_movement |= movement_down;
          } else if (current_event.key.keysym.sym == SDLK_e) {
            g_movement |= movement_up;
          }
        } break;
        case SDL_KEYUP: {
          if (current_event.key.keysym.sym == SDLK_w) {
            g_movement &= ~movement_forward;
          } else if (current_event.key.keysym.sym == SDLK_a) {
            g_movement &= ~movement_left;
          } else if (current_event.key.keysym.sym == SDLK_s) {
            g_movement &= ~movement_backward;
          } else if (current_event.key.keysym.sym == SDLK_d) {
            g_movement &= ~movement_right;
          } else if (current_event.key.keysym.sym == SDLK_q) {
            g_movement &= ~movement_down;
          } else if (current_event.key.keysym.sym == SDLK_e) {
            g_movement &= ~movement_up;
          }
        } break;
        default:
          break;
      }
    }

    update_movement((float)delta_time);

    const as_mat34f model = g_mode == mode_standard
                            ? g_model_transform
                            : as_mat34f_translation_from_vec3f((as_vec3f){0});
    const as_mat44f view = as_mat44f_from_mat34f_v(camera_view(&g_camera));
    const as_mat44f view_model =
      as_mat44f_mul_mat44f_v(view, as_mat44f_from_mat34f(&model));
    const as_mat44f orthographic_projection =
      as_mat44f_orthographic_projection_depth_minus_one_to_one_lh(
        -1.0f, 1.0f, -1.0f, 1.0f, 0.01f, 100.0f);

    ImGui_ImplSDL2_NewFrame();
    simgui_new_frame(&(simgui_frame_desc_t){
      .width = width,
      .height = height,
      .delta_time = delta_time,
      .dpi_scale = 1.0f});

    const float current_fov = fov_degrees;
    const float current_near_plane = near_plane;
    const float current_far_plane = far_plane;

    igSliderFloat("Field of view", &fov_degrees, 10.0f, 179.0f, "%.3f", 0);
    igSliderFloat("Near plane", &near_plane, 0.01f, 9.9f, "%.3f", 0);
    igSliderFloat("Far plane", &far_plane, 10.0f, 1000.0f, "%.3f", 0);

    const as_mat44f perspective_projection =
      as_mat44f_perspective_projection_depth_minus_one_to_one_lh(
        (float)width / (float)height, as_radians_from_degrees(fov_degrees),
        near_plane, far_plane);

    vs_params_lines.mvp = as_mat44f_transpose_v(
      g_mode == mode_standard
        ? as_mat44f_mul_mat44f(&perspective_projection, &view)
      : g_view == view_orthographic
        ? as_mat44f_mul_mat44f(&orthographic_projection, &view)
        : as_mat44f_mul_mat44f(&perspective_projection_projected_mode, &view));

    vs_params_model.mvp = as_mat44f_transpose_v(
      g_mode == mode_standard
        ? as_mat44f_mul_mat44f(&perspective_projection, &view_model)
      : g_view == view_orthographic
        ? as_mat44f_mul_mat44f(&orthographic_projection, &view_model)
        : as_mat44f_mul_mat44f(
          &perspective_projection_projected_mode, &view_model));

    mode_e prev_mode = g_mode;
    int mode_index = (int)g_mode;
    const char* mode_names[] = {"Standard", "Projected"};
    igCombo_Str_arr("Mode", &mode_index, mode_names, 2, 2);
    g_mode = (mode_e)mode_index;

    const bool projection_parameters_changed =
      !as_float_near(fov_degrees, current_fov, FLT_EPSILON, FLT_EPSILON)
      || !as_float_near(
        near_plane, current_near_plane, FLT_EPSILON, FLT_EPSILON)
      || !as_float_near(far_plane, current_far_plane, FLT_EPSILON, FLT_EPSILON);
    const bool mode_changed = g_mode != prev_mode;

    if (g_mode == mode_standard) {
      igBeginDisabled(true);
    }
    int view_index = (int)g_view;
    const char* view_names[] = {"Perspective", "Orthographic"};
    igCombo_Str_arr("View", &view_index, view_names, 2, 2);
    g_view = (view_e)view_index;
    if (g_mode == mode_standard) {
      igEndDisabled();
    }

    if (g_mode == mode_projected) {
      igBeginDisabled(true);
    }
    const bool camera_pinned = pin_camera;
    igCheckbox("Pin camera", &pin_camera);
    const bool pin_camera_changed = pin_camera != camera_pinned;
    if (g_mode == mode_projected) {
      igEndDisabled();
    }

    igCheckbox("Draw axes", &draw_axes);

    if (g_mode != mode_projected) {
      igBeginDisabled(true);
    }
    igCheckbox("Affine texture mapping", &g_affine);
    if (g_mode != mode_projected) {
      igEndDisabled();
    }

    if (!pin_camera) {
      pinned_camera_state.camera = g_camera;
      pinned_camera_state.fov_degrees = fov_degrees;
      pinned_camera_state.near_plane = near_plane;
      pinned_camera_state.far_plane = far_plane;
    } else {
      const frustum_corners_t frustum_corners = build_frustum_corners(
        (float)width / (float)height,
        as_radians_from_degrees(pinned_camera_state.fov_degrees),
        pinned_camera_state.near_plane, pinned_camera_state.far_plane);
      lines[0] = frustum_corners.corners[frustum_corner_near_bottom_left].x;
      lines[1] = frustum_corners.corners[frustum_corner_near_bottom_left].y;
      lines[2] = frustum_corners.corners[frustum_corner_near_bottom_left].z;
      lines[3] = frustum_corners.corners[frustum_corner_near_bottom_right].x;
      lines[4] = frustum_corners.corners[frustum_corner_near_bottom_right].y;
      lines[5] = frustum_corners.corners[frustum_corner_near_bottom_right].z;
      lines[6] = frustum_corners.corners[frustum_corner_near_top_right].x;
      lines[7] = frustum_corners.corners[frustum_corner_near_top_right].y;
      lines[8] = frustum_corners.corners[frustum_corner_near_top_right].z;
      lines[9] = frustum_corners.corners[frustum_corner_near_top_left].x;
      lines[10] = frustum_corners.corners[frustum_corner_near_top_left].y;
      lines[11] = frustum_corners.corners[frustum_corner_near_top_left].z;
      lines[12] = frustum_corners.corners[frustum_corner_far_bottom_left].x;
      lines[13] = frustum_corners.corners[frustum_corner_far_bottom_left].y;
      lines[14] = frustum_corners.corners[frustum_corner_far_bottom_left].z;
      lines[15] = frustum_corners.corners[frustum_corner_far_bottom_right].x;
      lines[16] = frustum_corners.corners[frustum_corner_far_bottom_right].y;
      lines[17] = frustum_corners.corners[frustum_corner_far_bottom_right].z;
      lines[18] = frustum_corners.corners[frustum_corner_far_top_right].x;
      lines[19] = frustum_corners.corners[frustum_corner_far_top_right].y;
      lines[20] = frustum_corners.corners[frustum_corner_far_top_right].z;
      lines[21] = frustum_corners.corners[frustum_corner_far_top_left].x;
      lines[22] = frustum_corners.corners[frustum_corner_far_top_left].y;
      lines[23] = frustum_corners.corners[frustum_corner_far_top_left].z;

      for (int corner_index = 0; corner_index < FrustumCornerCount;
           ++corner_index) {
        const as_point3f corner = frustum_corners.corners[corner_index];
        const as_point3f view_corner = as_mat34f_mul_point3f_v(
          camera_transform(&pinned_camera_state.camera), corner);
        lines[corner_index * 3 + 0] = view_corner.x;
        lines[corner_index * 3 + 1] = view_corner.y;
        lines[corner_index * 3 + 2] = view_corner.z;
      }
    }

    if (mode_changed || projection_parameters_changed || pin_camera_changed) {
      if (g_mode == mode_standard) {
        sg_destroy_buffer(line_buffer);
        line_buffer =
          sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(lines)});
        bind_line.vertex_buffers[0] = line_buffer;
      }

      if (g_mode == mode_projected) {
        if (mode_changed) {
          memcpy(&lines, unit_lines, sizeof(lines));
          sg_destroy_buffer(line_buffer);
          line_buffer =
            sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(lines)});
          bind_line.vertex_buffers[0] = line_buffer;

          projected_camera = pinned_camera_state.camera;

          g_camera.offset = (as_vec3f){0};
          g_camera.pivot = (as_point3f){0};
          g_camera.pitch = 0.0f;
          g_camera.yaw = 0.0f;
        }

        sg_destroy_buffer(projected_vertex_buffer);
        sg_destroy_buffer(vertex_depth_recip_buffer);

        const as_mat44f pinned_perspective_projection =
          as_mat44f_perspective_projection_depth_minus_one_to_one_lh(
            (float)width / (float)height,
            as_radians_from_degrees(pinned_camera_state.fov_degrees),
            pinned_camera_state.near_plane, pinned_camera_state.far_plane);

        for (int v = 0; v < array_length(projected_vertices); v += 3) {
          const as_point3f vertex = (as_point3f){
            model_vertices[v], model_vertices[v + 1], model_vertices[v + 2]};
          const as_point3f model_vertex =
            as_mat34f_mul_point3f_v(g_model_transform, vertex);
          const as_point3f model_view_vertex = as_mat34f_mul_point3f_v(
            camera_view(&projected_camera), model_vertex);
          const as_point4f projected_vertex = as_mat44f_project_point3f(
            &pinned_perspective_projection, model_view_vertex);
          projected_vertices[v] = projected_vertex.x;
          projected_vertices[v + 1] = projected_vertex.y;
          projected_vertices[v + 2] = projected_vertex.z;
        }

        projected_vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
          .data = (sg_range){
            .ptr = projected_vertices,
            .size = array_length(projected_vertices) * sizeof(float)}});

        for (int v = 0, d = 0; v < array_length(projected_vertices);
             v += 3, d++) {
          const as_point3f vertex = (as_point3f){
            model_vertices[v], model_vertices[v + 1], model_vertices[v + 2]};
          const as_point3f model_vertex =
            as_mat34f_mul_point3f_v(g_model_transform, vertex);
          const as_point3f model_view_vertex = as_mat34f_mul_point3f_v(
            camera_view(&projected_camera), model_vertex);
          vertex_depth_recips[d] = 1.0f / model_view_vertex.z;
        }

        vertex_depth_recip_buffer = sg_make_buffer(&(sg_buffer_desc){
          .data = (sg_range){
            .ptr = vertex_depth_recips,
            .size = array_length(vertex_depth_recips) * sizeof(float)}});

        bind_projected.vertex_buffers[0] = projected_vertex_buffer;
        bind_projected.vertex_buffers[2] = vertex_depth_recip_buffer;
      } else {
        if (mode_changed) {
          g_camera = projected_camera;
          // reset view in projected mode
          g_view = view_orthographic;
        }
      }
    }

    sg_bindings* bind =
      g_mode == mode_standard ? &bind_standard : &bind_projected;
    sg_pipeline pip = g_mode == mode_standard ? pip_standard
                    : g_affine                ? pip_projected_affine
                                              : pip_projected;

    sg_begin_default_pass(&pass_action, width, height);

    sg_apply_pipeline(pip);
    sg_apply_bindings(bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params_model));
    sg_draw(0, array_length(model_indices), 1);

    // only draw unit cube in projected mode
    if (g_mode == mode_projected || pin_camera || draw_axes) {
      sg_apply_pipeline(pip_line);
      sg_apply_bindings(&bind_line);
      sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params_lines));
      if (g_mode == mode_projected || pin_camera) {
        sg_draw(0, cube_line_indices_count, 1);
      }
      if (draw_axes) {
        sg_draw(cube_line_indices_count, axes_line_indices_count, 1);
      }
    }

    simgui_render();

    sg_end_pass();
    sg_commit();

    SDL_GL_SwapWindow(window);
  }

  sg_destroy_buffer(line_buffer);
  sg_destroy_buffer(line_color_buffer);
  sg_destroy_buffer(line_index_buffer);
  sg_destroy_buffer(standard_vertex_buffer);
  sg_destroy_buffer(projected_vertex_buffer);
  sg_destroy_buffer(uv_buffer);
  sg_destroy_buffer(vertex_depth_recip_buffer);
  sg_destroy_buffer(index_buffer);
  sg_destroy_shader(shader_standard);
  sg_destroy_shader(shader_projected);
  sg_destroy_shader(shader_line);
  sg_destroy_pipeline(pip_line);
  sg_destroy_pipeline(pip_standard);
  sg_destroy_pipeline(pip_projected);
  sg_destroy_pipeline(pip_projected_affine);
  sg_destroy_image(model_image);

  array_free(model_vertices);
  array_free(model_uvs);
  array_free(model_indices);

  upng_free(model.texture.png_texture);

  simgui_shutdown();
  sg_shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
