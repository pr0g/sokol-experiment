#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_NO_DEPRECATED
#include <glad/gl.h>
#include <sokol_gfx.h>

#include "imgui/imgui_impl_sdl.h"

#define SOKOL_IMGUI_NO_SOKOL_APP
#define SOKOL_IMGUI_IMPL
#include <util/sokol_imgui.h>

#include <SDL.h>
#include <as-ops.h>

#include "other/array.h"
#include "other/camera.h"
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
camera_t g_last_camera = {0};
int8_t g_movement = 0;
as_point2i g_mouse_position = {0};
bool g_mouse_down = false;
mode_e g_mode = mode_standard;
view_e g_view = view_orthographic;
as_mat34f g_model_transform = {0};

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

  const int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
  if (version == 0) {
    printf("Failed to initialize OpenGL context\n");
    return 1;
  }

  model_t model = load_obj_mesh_with_png_texture(
    "assets/models/f22.obj", "assets/textures/f22.png");

  // setup model data
  float* vertices = NULL;
  float* uvs = NULL;
  uint16_t* indices = NULL;
  uint16_t index = 0;
  const int face_count = array_length(model.mesh.faces);
  for (int f = 0; f < face_count; f++) {
    for (int v = 0; v < 3; v++) {
      const int vertex_index = model.mesh.faces[f].vert_indices[v] - 1;
      array_push(vertices, model.mesh.vertices[vertex_index].x);
      array_push(vertices, model.mesh.vertices[vertex_index].y);
      array_push(vertices, model.mesh.vertices[vertex_index].z);

      const int uv_index = model.mesh.faces[f].uv_indices[v] - 1;
      array_push(uvs, model.mesh.uvs[uv_index].u);
      array_push(uvs, 1.0f - model.mesh.uvs[uv_index].v);

      array_push(indices, index);
      index++;
    }
  }

  // setup sokol_gfx
  sg_setup(&(sg_desc){0});
  simgui_setup(&(simgui_desc_t){});
  ImGui_ImplSDL2_InitForOpenGL(window, context);

  g_model_transform = as_mat34f_translation_from_vec3f((as_vec3f){.z = 5.0f});

  const as_mat44f perspective_projection =
    as_mat44f_perspective_projection_depth_minus_one_to_one_lh(
      (float)width / (float)height, as_radians_from_degrees(60.0f), 2.0f,
      10.0f);

  const as_mat44f perspective_projection_projected_mode =
    as_mat44f_perspective_projection_depth_minus_one_to_one_lh(
      (float)width / (float)height, as_radians_from_degrees(60.0f), 0.01f,
      100.0f);

  float* projected_vertices = NULL;
  projected_vertices =
    array_hold(projected_vertices, array_length(vertices), sizeof(float));

  float* vertex_depth_recips = NULL;
  vertex_depth_recips =
    array_hold(vertex_depth_recips, array_length(vertices) / 3, sizeof(float));

  // clang-format off
  const float lines[] = {-1.0f, -1.0f, -1.0f,
                          1.0f, -1.0f, -1.0f,
                          1.0f, -1.0f, -1.0f,
                          1.0f,  1.0f, -1.0f,
                          1.0f,  1.0f, -1.0f,
                         -1.0f,  1.0f, -1.0f,
                         -1.0f,  1.0f, -1.0f,
                         -1.0f, -1.0f, -1.0f,

                         -1.0f, -1.0f, 1.0f,
                          1.0f, -1.0f, 1.0f,
                          1.0f, -1.0f, 1.0f,
                          1.0f,  1.0f, 1.0f,
                          1.0f,  1.0f, 1.0f,
                         -1.0f,  1.0f, 1.0f,
                         -1.0f,  1.0f, 1.0f,
                         -1.0f, -1.0f, 1.0f,

                         -1.0f, 1.0f, -1.0f,
                          1.0f, 1.0f, -1.0f,
                          1.0f, 1.0f, -1.0f,
                          1.0f, 1.0f,  1.0f,
                          1.0f, 1.0f,  1.0f,
                         -1.0f, 1.0f,  1.0f,
                         -1.0f, 1.0f,  1.0f,
                         -1.0f, 1.0f, -1.0f,

                         -1.0f, -1.0f, -1.0f,
                          1.0f, -1.0f, -1.0f,
                          1.0f, -1.0f, -1.0f,
                          1.0f, -1.0f,  1.0f,
                          1.0f, -1.0f,  1.0f,
                         -1.0f, -1.0f,  1.0f,
                         -1.0f, -1.0f,  1.0f,
                         -1.0f, -1.0f, -1.0f };
  const uint32_t line_colors[] = {0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,

                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,

                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,

                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff,
                                  0xffffffff};
  // clang-format on

  sg_buffer line_buffer =
    sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(lines)});
  sg_buffer line_color_buffer =
    sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(line_colors)});

  sg_buffer standard_vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){
      .ptr = vertices, .size = array_length(vertices) * sizeof(float)}});
  sg_buffer projected_vertex_buffer = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){
      .ptr = projected_vertices,
      .size = array_length(projected_vertices) * sizeof(float)}});
  sg_buffer uv_buffer = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){.ptr = uvs, .size = array_length(uvs) * sizeof(float)}});
  sg_buffer vertex_depth_recip_buffer = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){
      .ptr = vertex_depth_recips,
      .size = array_length(vertex_depth_recips) * sizeof(float)}});
  sg_buffer index_buffer = sg_make_buffer(&(sg_buffer_desc){
    .type = SG_BUFFERTYPE_INDEXBUFFER,
    .data = (sg_range){
      .ptr = indices, .size = array_length(indices) * sizeof(uint16_t)}});

  typedef struct vs_params_t {
    as_mat44f mvp;
  } vs_params_t;

  sg_shader shader_projected = sg_make_shader(&(sg_shader_desc){
    .fs.images[0] = {.name = "the_texture", .image_type = SG_IMAGETYPE_2D},
    .vs.uniform_blocks[0] =
      {.size = sizeof(vs_params_t),
       .uniforms = {[0] = {.name = "mvp", .type = SG_UNIFORMTYPE_MAT4}}},
    .vs.source = "#version 330\n"
                 "uniform mat4 mvp;\n"
                 "layout(location=0) in vec4 position;\n"
                 "layout(location=1) in vec2 uv0;\n"
                 "layout(location=2) in float depth_recip0;\n"
                 "out vec2 uv;\n"
                 "out float depth_recip;\n"
                 "void main() {\n"
                 "  gl_Position = mvp * position;\n"
                 "  uv = uv0 * depth_recip0;\n"
                 "  depth_recip = depth_recip0;\n"
                 "}\n",
    .fs.source = "#version 330\n"
                 "in vec2 uv;\n"
                 "in float depth_recip;"
                 "uniform sampler2D the_texture;\n"
                 "out vec4 frag_color;\n"
                 "void main() {\n"
                 "  frag_color = texture(the_texture, uv / depth_recip);\n"
                 "}\n"});

  sg_shader shader_standard = sg_make_shader(&(sg_shader_desc){
    .fs.images[0] = {.name = "the_texture", .image_type = SG_IMAGETYPE_2D},
    .vs.uniform_blocks[0] =
      {.size = sizeof(vs_params_t),
       .uniforms = {[0] = {.name = "mvp", .type = SG_UNIFORMTYPE_MAT4}}},
    .vs.source = "#version 330\n"
                 "uniform mat4 mvp;\n"
                 "layout(location=0) in vec4 position;\n"
                 "layout(location=1) in vec2 uv0;\n"
                 "out vec2 uv;\n"
                 "void main() {\n"
                 "  gl_Position = mvp * position;\n"
                 "  uv = uv0;\n"
                 "}\n",
    .fs.source = "#version 330\n"
                 "in vec2 uv;\n"
                 "uniform sampler2D the_texture;\n"
                 "out vec4 frag_color;\n"
                 "void main() {\n"
                 "  frag_color = texture(the_texture, uv);\n"
                 "}\n"});

  sg_shader shader_line = sg_make_shader(&(sg_shader_desc){
    .vs.uniform_blocks[0] =
      {.size = sizeof(vs_params_t),
       .uniforms = {[0] = {.name = "mvp", .type = SG_UNIFORMTYPE_MAT4}}},
    .vs.source = "#version 330\n"
                 "uniform mat4 mvp;\n"
                 "layout(location=0) in vec4 position;\n"
                 "layout(location=1) in vec4 color0;\n"
                 "out vec4 color;\n"
                 "void main() {\n"
                 "  gl_Position = mvp * position;\n"
                 "  color = color0;\n"
                 "}\n",
    .fs.source = "#version 330\n"
                 "in vec4 color;\n"
                 "out vec4 frag_color;\n"
                 "void main() {\n"
                 "  frag_color = color;\n"
                 "}\n"});

  sg_pipeline pip_projected = sg_make_pipeline(&(sg_pipeline_desc){
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
    .face_winding = SG_FACEWINDING_CW});

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
    .depth =
      {
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true,
      },
    .primitive_type = SG_PRIMITIVETYPE_LINES});

  // resource bindings
  sg_bindings bind_projected = {
    .vertex_buffers =
      {[0] = projected_vertex_buffer,
       [1] = uv_buffer,
       [2] = vertex_depth_recip_buffer},
    .vertex_buffer_offsets = {[0] = 0, [1] = 0, [2] = 0},
    .index_buffer = index_buffer,
    .fs_images[0] = sg_make_image(&(sg_image_desc){
      .width = model.texture.width,
      .height = model.texture.height,
      .data.subimage[0][0] =
        (sg_range){
          .ptr = model.texture.color_buffer,
          .size =
            model.texture.width * model.texture.height * sizeof(uint32_t)},
      .label = "model-texture"})};

  sg_bindings bind_standard = {
    .vertex_buffers = {[0] = standard_vertex_buffer, [1] = uv_buffer},
    .vertex_buffer_offsets = {[0] = 0, [1] = 0},
    .index_buffer = index_buffer,
    .fs_images[0] = sg_make_image(&(sg_image_desc){
      .width = model.texture.width,
      .height = model.texture.height,
      .data.subimage[0][0] =
        (sg_range){
          .ptr = model.texture.color_buffer,
          .size =
            model.texture.width * model.texture.height * sizeof(uint32_t)},
      .label = "model-texture"})};

  sg_bindings bind_line = {
    .vertex_buffers = {[0] = line_buffer, [1] = line_color_buffer},
    .vertex_buffer_offsets = {[0] = 0, [1] = 0}};

  // default pass action (clear to grey)
  sg_pass_action pass_action = {0};

  vs_params_t vs_params;
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
          if (current_event.key.keysym.sym == SDLK_p) {
            if (g_mode == mode_standard) {
              g_mode = mode_projected;

              sg_destroy_buffer(projected_vertex_buffer);
              sg_destroy_buffer(vertex_depth_recip_buffer);

              for (int v = 0; v < array_length(projected_vertices); v += 3) {
                const as_point3f vertex =
                  (as_point3f){vertices[v], vertices[v + 1], vertices[v + 2]};
                const as_point3f model_vertex =
                  as_mat34f_mul_point3f_v(g_model_transform, vertex);
                const as_point3f model_view_vertex =
                  as_mat34f_mul_point3f_v(camera_view(&g_camera), model_vertex);
                const as_point4f projected_vertex = as_mat44f_project_point3f(
                  &perspective_projection, model_view_vertex);
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
                const as_point3f vertex =
                  (as_point3f){vertices[v], vertices[v + 1], vertices[v + 2]};
                const as_point3f model_vertex =
                  as_mat34f_mul_point3f_v(g_model_transform, vertex);
                const as_point3f model_view_vertex =
                  as_mat34f_mul_point3f_v(camera_view(&g_camera), model_vertex);
                vertex_depth_recips[d] = 1.0f / model_view_vertex.z;
              }

              vertex_depth_recip_buffer = sg_make_buffer(&(sg_buffer_desc){
                .data = (sg_range){
                  .ptr = vertex_depth_recips,
                  .size = array_length(vertex_depth_recips) * sizeof(float)}});

              bind_projected.vertex_buffers[0] = projected_vertex_buffer;
              bind_projected.vertex_buffers[2] = vertex_depth_recip_buffer;

              g_last_camera = g_camera;
              g_camera.offset = (as_vec3f){0};
              g_camera.pivot = (as_point3f){0};
              g_camera.pitch = 0.0f;
              g_camera.yaw = 0.0f;

            } else {
              g_camera = g_last_camera;
              g_mode = mode_standard;
              // reset view in projected mode
              g_view = view_orthographic;
            }
          }
          if (current_event.key.keysym.sym == SDLK_v) {
            if (g_view == view_orthographic) {
              g_view = view_perspective;
            } else {
              g_view = view_orthographic;
            }
          }
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
    const as_mat44f view_model = as_mat44f_from_mat34f_v(
      as_mat34f_mul_mat34f_v(camera_view(&g_camera), model));
    const as_mat44f orthographic_projection =
      as_mat44f_orthographic_projection_depth_minus_one_to_one_lh(
        -1.0f, 1.0f, -1.0f, 1.0f, 0.01f, 100.0f);

    vs_params.mvp = as_mat44f_transpose_v(
      g_mode == mode_standard
        ? as_mat44f_mul_mat44f(&perspective_projection, &view_model)
      : g_view == view_orthographic
        ? as_mat44f_mul_mat44f(&orthographic_projection, &view_model)
        : as_mat44f_mul_mat44f(
          &perspective_projection_projected_mode, &view_model));

    ImGui_ImplSDL2_NewFrame();
    simgui_new_frame(&(simgui_frame_desc_t){
      .width = width,
      .height = height,
      .delta_time = delta_time,
      .dpi_scale = 1.0f});

    static bool open = false;
    igShowDemoWindow(&open);

    sg_bindings* bind =
      g_mode == mode_standard ? &bind_standard : &bind_projected;
    sg_pipeline pip = g_mode == mode_standard ? pip_standard : pip_projected;

    sg_begin_default_pass(&pass_action, width, height);

    sg_apply_pipeline(pip);
    sg_apply_bindings(bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
    sg_draw(0, array_length(indices), 1);

    // only draw unit cube in projected mode
    if (g_mode == mode_projected) {
      sg_apply_pipeline(pip_line);
      sg_apply_bindings(&bind_line);
      sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
      sg_draw(0, sizeof(lines) / sizeof(float) / 2, 1);
    }

    simgui_render();

    sg_end_pass();
    sg_commit();

    SDL_GL_SwapWindow(window);
  }

  simgui_shutdown();
  sg_shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
