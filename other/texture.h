#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>
#include <upng.h>

typedef struct tex2f_t {
  float u;
  float v;
} tex2f_t;

typedef struct texture_t {
  upng_t* png_texture;
  uint32_t* color_buffer;
  int width;
  int height;
} texture_t;

texture_t load_png_texture(const char* filename);

#endif // TEXTURE_H
