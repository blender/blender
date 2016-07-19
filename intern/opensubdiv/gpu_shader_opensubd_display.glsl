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

#ifdef VERTEX_SHADER // ---------------------

in vec3 normal;
in vec4 position;

uniform mat4 modelViewMatrix;
uniform mat3 normalMatrix;

out block {
	VertexData v;
} outpt;

void main()
{
	outpt.v.position = modelViewMatrix * position;
	outpt.v.normal = normalize(normalMatrix * normal);

#if __VERSION__ < 140
	/* Some compilers expects gl_Position to be written.
	 * It's not needed once we explicitly switch to GLSL 1.40 or above.
	 */
	gl_Position = outpt.v.position;
#endif
}

#elif defined GEOMETRY_SHADER // ---------------------

#if __VERSION__ >= 150
  layout(lines_adjacency) in;
  #ifdef WIREFRAME
    layout(line_strip, max_vertices = 8) out;
  #else
    layout(triangle_strip, max_vertices = 4) out;
  #endif
#else
  #extension GL_EXT_geometry_shader4: require
  /* application provides input/output layout info */
#endif

#if __VERSION__ < 140
  #extension GL_ARB_uniform_buffer_object: require
  #extension GL_ARB_texture_buffer_object: enable
  #extension GL_EXT_texture_buffer_object: enable
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

#elif defined FRAGMENT_SHADER // ---------------------

#define MAX_LIGHTS 8
#define NUM_SOLID_LIGHTS 3

struct LightSource {
	vec4 position;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	vec4 spotDirection;
#ifdef SUPPORT_COLOR_MATERIAL
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
	float spotCutoff;
	float spotExponent;
	float spotCosCutoff;
	float pad, pad2;
#endif
};

layout(std140) uniform Lighting {
	LightSource lightSource[MAX_LIGHTS];
	int num_enabled_lights;
};

uniform vec4 diffuse;
uniform vec4 specular;
uniform float shininess;

uniform sampler2D texture_buffer;

in block {
	VertexData v;
} inpt;

void main()
{
#ifdef WIREFRAME
	gl_FragColor = diffuse;
#else
	vec3 N = inpt.v.normal;

	if (!gl_FrontFacing)
		N = -N;

	/* Compute diffuse and specular lighting. */
	vec3 L_diffuse = vec3(0.0);
	vec3 L_specular = vec3(0.0);

#ifndef USE_COLOR_MATERIAL
	/* Assume NUM_SOLID_LIGHTS directional lights. */
	for (int i = 0; i < NUM_SOLID_LIGHTS; i++) {
		vec4 Plight = lightSource[i].position;
#ifdef USE_DIRECTIONAL_LIGHT
		vec3 l = (Plight.w == 0.0)
		            ? normalize(Plight.xyz)
		            : normalize(inpt.v.position.xyz);
#else  /* USE_DIRECTIONAL_LIGHT */
		/* TODO(sergey): We can normalize it outside of the shader. */
		vec3 l = normalize(Plight.xyz);
#endif  /* USE_DIRECTIONAL_LIGHT */
		vec3 h = normalize(l + vec3(0, 0, 1));
		float d = max(0.0, dot(N, l));
		float s = pow(max(0.0, dot(N, h)), shininess);
		L_diffuse += d * lightSource[i].diffuse.rgb;
		L_specular += s * lightSource[i].specular.rgb;
	}
#else  /* USE_COLOR_MATERIAL */
	vec3 varying_position = inpt.v.position.xyz;
	vec3 V = (gl_ProjectionMatrix[3][3] == 0.0) ?
	         normalize(varying_position) : vec3(0.0, 0.0, -1.0);
	for (int i = 0; i < num_enabled_lights; i++) {
		/* todo: this is a slow check for disabled lights */
		if (lightSource[i].specular.a == 0.0)
			continue;

		float intensity = 1.0;
		vec3 light_direction;

		if (lightSource[i].position.w == 0.0) {
			/* directional light */
			light_direction = lightSource[i].position.xyz;
		}
		else {
			/* point light */
			vec3 d = lightSource[i].position.xyz - varying_position;
			light_direction = normalize(d);

			/* spot light cone */
			if (lightSource[i].spotCutoff < 90.0) {
				float cosine = max(dot(light_direction,
				                       -lightSource[i].spotDirection.xyz),
				                   0.0);
				intensity = pow(cosine, lightSource[i].spotExponent);
				intensity *= step(lightSource[i].spotCosCutoff, cosine);
			}

			/* falloff */
			float distance = length(d);

			intensity /= lightSource[i].constantAttenuation +
				lightSource[i].linearAttenuation * distance +
				lightSource[i].quadraticAttenuation * distance * distance;
		}

		/* diffuse light */
		vec3 light_diffuse = lightSource[i].diffuse.rgb;
		float diffuse_bsdf = max(dot(N, light_direction), 0.0);
		L_diffuse += light_diffuse * diffuse_bsdf * intensity;

		/* specular light */
		vec3 light_specular = lightSource[i].specular.rgb;
		vec3 H = normalize(light_direction - V);

		float specular_bsdf = pow(max(dot(N, H), 0.0),
		                          gl_FrontMaterial.shininess);
		L_specular += light_specular * specular_bsdf * intensity;
	}
#endif  /* USE_COLOR_MATERIAL */

	/* Compute diffuse color. */
#ifdef USE_TEXTURE_2D
	L_diffuse *= texture2D(texture_buffer, inpt.v.uv).rgb;
#else
	L_diffuse *= diffuse.rgb;
#endif

	/* Sum lighting. */
	vec3 L = L_diffuse;
	if (shininess != 0) {
		L += L_specular * specular.rgb;
	}

	/* Write out fragment color. */
	gl_FragColor = vec4(L, diffuse.a);
#endif
}

#endif // ---------------------
