void main()
{
  int index = int(gl_GlobalInvocationID.x);
  vec4 pos = vec4(gl_GlobalInvocationID.x);
  imageStore(img_output, index, pos);
}
