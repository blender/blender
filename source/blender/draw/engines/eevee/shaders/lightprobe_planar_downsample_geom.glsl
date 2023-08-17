
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

void main()
{
  gl_Layer = lightprobe_vert_iface_flat[0].instance;
  lightprobe_geom_iface.layer = float(lightprobe_vert_iface_flat[0].instance);

  gl_Position = vec4(lightprobe_vert_iface[0].vPos, 0.0, 1.0);
  EmitVertex();

  gl_Position = vec4(lightprobe_vert_iface[1].vPos, 0.0, 1.0);
  EmitVertex();

  gl_Position = vec4(lightprobe_vert_iface[2].vPos, 0.0, 1.0);
  EmitVertex();

  EndPrimitive();
}
