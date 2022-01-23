#ifndef USE_GPU_SHADER_CREATE_INFO
flat in uint finalId;
out uint fragId;
#endif

void main()
{
  fragId = finalId;
}
