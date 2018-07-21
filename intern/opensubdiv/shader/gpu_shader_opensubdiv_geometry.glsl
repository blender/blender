/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

struct VertexData {
	vec4 position;
	vec3 normal;
	vec2 uv;
};

layout(lines_adjacency) in;
#ifdef WIREFRAME
layout(line_strip, max_vertices = 8) out;
#else
layout(triangle_strip, max_vertices = 4) out;
#endif

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform int PrimitiveIdBase;
uniform int osd_fvar_count;
uniform int osd_active_uv_offset;

in block {
	VertexData v;
} inpt[];

#define INTERP_FACE_VARYING_2(result, fvarOffset, tessCoord)  \
	{ \
		vec2 v[4]; \
		int primOffset = (gl_PrimitiveID + PrimitiveIdBase) * 4; \
		for (int i = 0; i < 4; ++i) { \
			int index = (primOffset + i) * osd_fvar_count + fvarOffset; \
			v[i] = vec2(texelFetch(FVarDataBuffer, index).s, \
			            texelFetch(FVarDataBuffer, index + 1).s); \
		} \
		result = mix(mix(v[0], v[1], tessCoord.s), \
		             mix(v[3], v[2], tessCoord.s), \
		             tessCoord.t); \
	}

uniform samplerBuffer FVarDataBuffer;
uniform isamplerBuffer FVarDataOffsetBuffer;

out block {
	VertexData v;
} outpt;

#ifdef FLAT_SHADING
void emit(int index, vec3 normal)
{
	outpt.v.position = inpt[index].v.position;
	outpt.v.normal = normal;

	/* TODO(sergey): Only uniform subdivisions atm. */
	vec2 quadst[4] = vec2[](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
	vec2 st = quadst[index];

	INTERP_FACE_VARYING_2(outpt.v.uv, osd_active_uv_offset, st);

	gl_Position = projectionMatrix * inpt[index].v.position;
	EmitVertex();
}

#  ifdef WIREFRAME
void emit_edge(int v0, int v1, vec3 normal)
{
	emit(v0, normal);
	emit(v1, normal);
}
#  endif

#else
void emit(int index)
{
	outpt.v.position = inpt[index].v.position;
	outpt.v.normal = inpt[index].v.normal;

	/* TODO(sergey): Only uniform subdivisions atm. */
	vec2 quadst[4] = vec2[](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
	vec2 st = quadst[index];

	INTERP_FACE_VARYING_2(outpt.v.uv, osd_active_uv_offset, st);

	gl_Position = projectionMatrix * inpt[index].v.position;
	EmitVertex();
}

#  ifdef WIREFRAME
void emit_edge(int v0, int v1)
{
	emit(v0);
	emit(v1);
}
#  endif

#endif

void main()
{
	gl_PrimitiveID = gl_PrimitiveIDIn;

#ifdef FLAT_SHADING
	vec3 A = (inpt[0].v.position - inpt[1].v.position).xyz;
	vec3 B = (inpt[3].v.position - inpt[1].v.position).xyz;
	vec3 flat_normal = normalize(cross(B, A));
#  ifndef WIREFRAME
	emit(0, flat_normal);
	emit(1, flat_normal);
	emit(3, flat_normal);
	emit(2, flat_normal);
#  else
	emit_edge(0, 1, flat_normal);
	emit_edge(1, 2, flat_normal);
	emit_edge(2, 3, flat_normal);
	emit_edge(3, 0, flat_normal);
#  endif
#else
#  ifndef WIREFRAME
	emit(0);
	emit(1);
	emit(3);
	emit(2);
#  else
	emit_edge(0, 1);
	emit_edge(1, 2);
	emit_edge(2, 3);
	emit_edge(3, 0);
#  endif
#endif

	EndPrimitive();
}
