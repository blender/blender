void main()
{
  int store_index = int(gl_GlobalInvocationID.x);
  data_out[store_index] = store_index * 4;
}
