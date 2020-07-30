
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#ifdef MESH_SHADER
/* TODO tight slices */
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
#else /* World */
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
#endif

in vec4 vPos[];

flat out int slice;

RESOURCE_ID_VARYING

#ifdef MESH_SHADER
/* TODO tight slices */
void main()
{
  gl_Layer = slice = int(vPos[0].z);

  PASS_RESOURCE_ID

#  ifdef USE_ATTR
  pass_attr(0);
#  endif
  gl_Position = vPos[0].xyww;
  EmitVertex();

#  ifdef USE_ATTR
  pass_attr(1);
#  endif
  gl_Position = vPos[1].xyww;
  EmitVertex();

#  ifdef USE_ATTR
  pass_attr(2);
#  endif
  gl_Position = vPos[2].xyww;
  EmitVertex();

  EndPrimitive();
}

#else /* World */

/* This is just a pass-through geometry shader that send the geometry
 * to the layer corresponding to it's depth. */

void main()
{
  gl_Layer = slice = int(vPos[0].z);

  PASS_RESOURCE_ID

#  ifdef USE_ATTR
  pass_attr(0);
#  endif
  gl_Position = vPos[0].xyww;
  EmitVertex();

#  ifdef USE_ATTR
  pass_attr(1);
#  endif
  gl_Position = vPos[1].xyww;
  EmitVertex();

#  ifdef USE_ATTR
  pass_attr(2);
#  endif
  gl_Position = vPos[2].xyww;
  EmitVertex();

  EndPrimitive();
}

#endif
