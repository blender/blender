void main() {
  uint index = gl_GlobalInvocationID.x;
  vec4 pos = vec4(gl_GlobalInvocationID.x);
  out_positions[index] = pos;
}