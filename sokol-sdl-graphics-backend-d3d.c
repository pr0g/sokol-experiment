#include "sokol-sdl-graphics-backend.h"

#define SOKOL_D3D11
#define SOKOL_NO_DEPRECATED
#include <sokol_gfx.h>

#define COBJMACROS
#include <d3d11.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include <as-ops.h>

ID3D11Device* g_d3d_device = NULL;
ID3D11DeviceContext* g_d3d_device_context = NULL;
IDXGISwapChain* g_swap_chain = NULL;
ID3D11RenderTargetView* g_render_target_view = NULL;
ID3D11DepthStencilView* g_depth_stencil_view = NULL;

bool create_device_d3d(HWND h_wnd);
void cleanup_device_d3d(void);
void create_render_target(void);
void cleanup_render_target(void);
void cleanup_depth_stencil(void);

static const void* d3d11_render_target_view(void) {
  return (const void*)g_render_target_view;
}

static const void* d3d11_depth_stencil_view(void) {
  return (const void*)g_depth_stencil_view;
}

////////////////////////////////////////////////////////////////////////////////

bool se_init_backend(SDL_Window* window) {
  SDL_SysWMinfo wm_info;
  SDL_VERSION(&wm_info.version);
  SDL_GetWindowWMInfo(window, &wm_info);
  const HWND hwnd = (HWND)wm_info.info.win.window;
  if (!create_device_d3d(hwnd)) {
    cleanup_device_d3d();
    return false;
  }
  return true;
}

sg_desc se_create_desc() {
  return (sg_desc){
    .context = (sg_context_desc){
      .d3d11 = {
        .device = g_d3d_device,
        .device_context = g_d3d_device_context,
        .render_target_view_cb = d3d11_render_target_view,
        .depth_stencil_view_cb = d3d11_depth_stencil_view}}};
}

void se_init_imgui(SDL_Window* window) {
  ImGui_ImplSDL2_InitForD3D(window);
}

as_mat44f se_perspective_projection(
  float aspect_ratio, float vertical_fov_radians, float near_plane,
  float far_plane) {
  return as_mat44f_perspective_projection_depth_zero_to_one_lh(
    aspect_ratio, vertical_fov_radians, near_plane, far_plane);
}

as_mat44f se_orthographic_projection(
  float left, float right, float bottom, float top, float near_plane,
  float far_plane) {
  return as_mat44f_orthographic_projection_depth_zero_to_one_lh(
    left, right, bottom, top, near_plane, far_plane);
}

void se_present(SDL_Window* window) {
  (void)window;
  IDXGISwapChain_Present(g_swap_chain, 1, 0);
}

void se_deinit_backend() {
 cleanup_device_d3d();   
}

////////////////////////////////////////////////////////////////////////////////

bool create_device_d3d(HWND h_wnd) {
  DXGI_SWAP_CHAIN_DESC sd = {0};
  sd.BufferCount = 1;
  sd.BufferDesc.Width = 1024;
  sd.BufferDesc.Height = 768;
  sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = h_wnd;
  sd.SampleDesc.Count = 4;
  sd.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT create_device_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
  D3D_FEATURE_LEVEL feature_level;

  if (
    D3D11CreateDeviceAndSwapChain(
      NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_device_flags, NULL, 0,
      D3D11_SDK_VERSION, &sd, &g_swap_chain, &g_d3d_device, &feature_level,
      &g_d3d_device_context)
    != S_OK) {
    return false;
  }

  create_render_target();
  return true;
}

void cleanup_device_d3d(void) {
  cleanup_depth_stencil();
  cleanup_render_target();
  if (g_swap_chain) {
    IDXGISwapChain_Release(g_swap_chain);
    g_swap_chain = NULL;
  }
  if (g_d3d_device_context) {
    ID3D11DeviceContext_Release(g_d3d_device_context);
    g_d3d_device_context = NULL;
  }
  if (g_d3d_device) {
    ID3D11Device_Release(g_d3d_device);
    g_d3d_device = NULL;
  }
}

void create_render_target(void) {
  ID3D11Texture2D* back_buffer = NULL;
  IDXGISwapChain_GetBuffer(
    g_swap_chain, 0, &IID_ID3D11Texture2D, (LPVOID*)&back_buffer);
  ID3D11Device_CreateRenderTargetView(
    g_d3d_device, (ID3D11Resource*)back_buffer, NULL, &g_render_target_view);
  ID3D11Texture2D_Release(back_buffer);

  D3D11_TEXTURE2D_DESC ds_desc = {
    .Width = 1024,
    .Height = 768,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
    .SampleDesc = {.Count = 4, .Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_DEPTH_STENCIL,
  };
  ID3D11Texture2D* depth_stencil_buffer = NULL;
  ID3D11Device_CreateTexture2D(
    g_d3d_device, &ds_desc, NULL, &depth_stencil_buffer);
  D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
    .Format = ds_desc.Format, .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS};
  ID3D11Device_CreateDepthStencilView(
    g_d3d_device, (ID3D11Resource*)depth_stencil_buffer, &dsv_desc,
    &g_depth_stencil_view);
  ID3D11Texture2D_Release(depth_stencil_buffer);
}

void cleanup_render_target(void) {
  if (g_render_target_view) {
    ID3D11RenderTargetView_Release(g_render_target_view);
    g_render_target_view = NULL;
  }
}

void cleanup_depth_stencil(void) {
  if (g_depth_stencil_view) {
    ID3D11DepthStencilView_Release(g_depth_stencil_view);
    g_depth_stencil_view = NULL;
  }
}