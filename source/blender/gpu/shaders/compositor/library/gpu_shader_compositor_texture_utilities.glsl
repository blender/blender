/* A shorthand for 1D textureSize with a zero LOD. */
int texture_size(sampler1D sampler)
{
  return textureSize(sampler, 0);
}

/* A shorthand for 1D texelFetch with zero LOD and bounded access clamped to border. */
vec4 texture_load(sampler1D sampler, int x)
{
  const int texture_bound = texture_size(sampler) - 1;
  return texelFetch(sampler, clamp(x, 0, texture_bound), 0);
}

/* A shorthand for 2D textureSize with a zero LOD. */
ivec2 texture_size(sampler2D sampler)
{
  return textureSize(sampler, 0);
}

/* A shorthand for 2D texelFetch with zero LOD and bounded access clamped to border. */
vec4 texture_load(sampler2D sampler, ivec2 texel)
{
  const ivec2 texture_bounds = texture_size(sampler) - ivec2(1);
  return texelFetch(sampler, clamp(texel, ivec2(0), texture_bounds), 0);
}
