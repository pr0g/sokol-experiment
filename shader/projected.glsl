@vs projected_vs
uniform ProjectedUniforms {
  mat4 mvp;
};
layout(location=0) in vec4 position;
layout(location=1) in vec2 uv0;
layout(location=2) in float depth_recip0;
out vec2 uv;
out float depth_recip;
void main() {
  gl_Position = mvp * position;
  uv = uv0 * depth_recip0;
  depth_recip = depth_recip0;
}
@end

@fs projected_fs
in vec2 uv;
in float depth_recip;
uniform sampler2D the_texture;
out vec4 frag_color;
void main() {
  frag_color = texture(the_texture, uv / depth_recip);
}
@end

@program projected projected_vs projected_fs
