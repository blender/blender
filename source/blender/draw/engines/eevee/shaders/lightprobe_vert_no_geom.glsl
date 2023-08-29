/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Non-geometry shader equivalent for eevee_legacy_lightprobe_vert + eevee_legacy_lightprobe_geom.
 * generates geometry instance per cube-face for multi-layered rendering. */
void main()
{

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
  geom_iface_flat.fFace = gl_InstanceID;
  gl_Position = vec4(pos.xyz, 1.0);
  geom_iface.worldPosition = x_axis[geom_iface_flat.fFace] * pos.x +
                             y_axis[geom_iface_flat.fFace] * pos.y +
                             maj_axes[geom_iface_flat.fFace];

#ifdef GPU_METAL
  /* In the Metal API, gl_Layer equivalent is specified in the vertex shader for multilayered
   * rendering support. */
  gpu_Layer = Layer + geom_iface_flat.fFace;
#endif
}
