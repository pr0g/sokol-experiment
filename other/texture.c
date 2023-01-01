#include "texture.h"

#include <stdio.h>

texture_t load_png_texture(const char* filename) {
  upng_t* texture = upng_new_from_file(filename);
  if (texture != NULL) {
    upng_decode(texture);
    if (upng_get_error(texture) == UPNG_EOK) {
      return (texture_t){
        .png_texture = texture,
        .color_buffer = (uint32_t*)upng_get_buffer(texture),
        .width = (int)upng_get_width(texture),
        .height = (int)upng_get_height(texture)};
    }
  }
  return (texture_t){0};
}
