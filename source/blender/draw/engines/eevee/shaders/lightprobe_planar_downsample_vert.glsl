
void main()
{
  int v = gl_VertexID % 3;
  lightprobe_vert_iface.vPos.x = -1.0 + float((v & 1) << 2);
  lightprobe_vert_iface.vPos.y = -1.0 + float((v & 2) << 1);

  lightprobe_vert_iface.instance = gl_VertexID / 3;

#ifdef GPU_METAL
  gpu_Layer = lightprobe_vert_iface.instance;
  lightprobe_geom_iface.layer = float(lightprobe_vert_iface.instance);
  gl_Position = vec4(lightprobe_vert_iface.vPos, 0.0, 1.0);
#endif
}
