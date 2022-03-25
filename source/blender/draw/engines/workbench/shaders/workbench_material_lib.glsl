
void workbench_material_data_get(
    int handle, out vec3 color, out float alpha, out float roughness, out float metallic)
{
  handle = (materialIndex != -1) ? materialIndex : handle;
  vec4 data = materials_data[uint(handle) & 0xFFFu];
  color = data.rgb;

  uint encoded_data = floatBitsToUint(data.w);
  alpha = float((encoded_data >> 16u) & 0xFFu) * (1.0 / 255.0);
  roughness = float((encoded_data >> 8u) & 0xFFu) * (1.0 / 255.0);
  metallic = float(encoded_data & 0xFFu) * (1.0 / 255.0);
}
