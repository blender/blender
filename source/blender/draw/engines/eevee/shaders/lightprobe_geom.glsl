
const vec3 maj_axes[6] = vec3[6](vec3(1.0, 0.0, 0.0),
                                 vec3(-1.0, 0.0, 0.0),
                                 vec3(0.0, 1.0, 0.0),
                                 vec3(0.0, -1.0, 0.0),
                                 vec3(0.0, 0.0, 1.0),
                                 vec3(0.0, 0.0, -1.0));
const vec3 x_axis[6] = vec3[6](vec3(0.0, 0.0, -1.0),
                               vec3(0.0, 0.0, 1.0),
                               vec3(1.0, 0.0, 0.0),
                               vec3(1.0, 0.0, 0.0),
                               vec3(1.0, 0.0, 0.0),
                               vec3(-1.0, 0.0, 0.0));
const vec3 y_axis[6] = vec3[6](vec3(0.0, -1.0, 0.0),
                               vec3(0.0, -1.0, 0.0),
                               vec3(0.0, 0.0, 1.0),
                               vec3(0.0, 0.0, -1.0),
                               vec3(0.0, -1.0, 0.0),
                               vec3(0.0, -1.0, 0.0));

void main()
{
  geom_iface_flat.fFace = vert_iface_flat[0].face;
  gl_Layer = Layer + geom_iface_flat.fFace;

  for (int v = 0; v < 3; v++) {
    gl_Position = vert_iface[v].vPos;
    geom_iface.worldPosition = x_axis[geom_iface_flat.fFace] * vert_iface[v].vPos.x +
                               y_axis[geom_iface_flat.fFace] * vert_iface[v].vPos.y +
                               maj_axes[geom_iface_flat.fFace];
    EmitVertex();
  }

  EndPrimitive();
}
