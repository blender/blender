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
