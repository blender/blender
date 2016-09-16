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

#ifdef USE_LIGHTING
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
#else  /* USE_LIGHTING */
	L_diffuse = vec3(1.0);
#endif

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
