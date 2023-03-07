@vs line_vs
uniform LineUniforms {
  mat4 mvp;
};
layout(location=0) in vec4 position;
layout(location=1) in vec4 color0;
out vec4 color;
void main() {
  gl_Position = mvp * position;
  color = color0;
}
@end

@fs line_fs
in vec4 color;
out vec4 frag_color;
void main() {
  frag_color = color;
}
@end

@program line line_vs line_fs
