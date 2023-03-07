@vs standard_vs
uniform StandardUniforms {
  mat4 mvp;
};
layout(location=0) in vec4 position;
layout(location=1) in vec2 uv0;
out vec2 uv;
void main() {
  gl_Position = mvp * position;
  uv = uv0;
}
@end

@fs standard_fs
in vec2 uv;
uniform sampler2D the_texture;
out vec4 frag_color;
void main() {
  frag_color = texture(the_texture, uv);
}
@end

@program standard standard_vs standard_fs
