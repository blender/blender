void main()
{
  int index = int(gl_GlobalInvocationID.x);
  float value = texelFetch(bufferTexture, index).r;
  data_out[index] = value;
}
