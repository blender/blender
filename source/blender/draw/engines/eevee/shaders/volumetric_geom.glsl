/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#ifdef MESH_SHADER
/* TODO: tight slices. */
void main()
{
  gl_Layer = volumetric_geom_iface.slice = int(volumetric_vert_iface[0].vPos.z);

  PASS_RESOURCE_ID

#  ifdef USE_ATTR
  pass_attr(0);
#  endif
  gl_Position = volumetric_vert_iface[0].vPos.xyww;
  EmitVertex();

#  ifdef USE_ATTR
  pass_attr(1);
#  endif
  gl_Position = volumetric_vert_iface[1].vPos.xyww;
  EmitVertex();

#  ifdef USE_ATTR
  pass_attr(2);
#  endif
  gl_Position = volumetric_vert_iface[2].vPos.xyww;
  EmitVertex();

  EndPrimitive();
}

#else /* World */

/* This is just a pass-through geometry shader that send the geometry
 * to the layer corresponding to its depth. */

void main()
{
  gl_Layer = volumetric_geom_iface.slice = int(volumetric_vert_iface[0].vPos.z);

  PASS_RESOURCE_ID

#  ifdef USE_ATTR
  pass_attr(0);
#  endif
  gl_Position = volumetric_vert_iface[0].vPos.xyww;
  EmitVertex();

#  ifdef USE_ATTR
  pass_attr(1);
#  endif
  gl_Position = volumetric_vert_iface[1].vPos.xyww;
  EmitVertex();

#  ifdef USE_ATTR
  pass_attr(2);
#  endif
  gl_Position = volumetric_vert_iface[2].vPos.xyww;
  EmitVertex();

  EndPrimitive();
}

#endif
