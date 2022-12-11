#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include <glad/gl.h>
#include <sokol_gfx.h>

#include <SDL.h>

int main(int argc, char** argv)
{
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

  const SDL_GLContext context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, context);

  const int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
  if (version == 0) {
    printf("Failed to initialize OpenGL context\n");
    return 1;
  }

  // setup sokol_gfx
  sg_setup(&(sg_desc){0});

  // a vertex buffer
  const float vertices[] = {// positions            // colors
                            0.0f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
                            0.5f,  -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
                            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f};
  sg_buffer vbuf =
    sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices)});

  sg_shader shd = sg_make_shader(&(sg_shader_desc){
    .vs.source = "#version 330\n"
                 "layout(location=0) in vec4 position;\n"
                 "layout(location=1) in vec4 color0;\n"
                 "out vec4 color;\n"
                 "void main() {\n"
                 "  gl_Position = position;\n"
                 "  color = color0;\n"
                 "}\n",
    .fs.source = "#version 330\n"
                 "in vec4 color;\n"
                 "out vec4 frag_color;\n"
                 "void main() {\n"
                 "  frag_color = color;\n"
                 "}\n"});

  // a pipeline state object (default render states are fine for triangle)
  sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
    .shader = shd,
    .layout = {
      .attrs = {
        [0].format = SG_VERTEXFORMAT_FLOAT3,
        [1].format = SG_VERTEXFORMAT_FLOAT4}}});

  // resource bindings
  sg_bindings bind = {.vertex_buffers[0] = vbuf};

  // default pass action (clear to grey)
  sg_pass_action pass_action = {0};

  for (bool quit = false; !quit;) {
    for (SDL_Event current_event; SDL_PollEvent(&current_event) != 0;) {
      if (current_event.type == SDL_QUIT) {
        quit = true;
        break;
      }
    }

    sg_begin_default_pass(&pass_action, width, height);
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();

    SDL_GL_SwapWindow(window);
  }

  sg_shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();
}
