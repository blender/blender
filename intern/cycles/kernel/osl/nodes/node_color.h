/*
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
 */

/* Color Management */

float color_srgb_to_scene_linear(float c)
{
	if(c < 0.04045)
		return (c < 0.0)? 0.0: c * (1.0/12.92);
	else
		return pow((c + 0.055)*(1.0/1.055), 2.4);
}

float color_scene_linear_to_srgb(float c)
{
	if(c < 0.0031308)
		return (c < 0.0)? 0.0: c * 12.92;
    else
		return 1.055 * pow(c, 1.0/2.4) - 0.055;
}

color color_srgb_to_scene_linear(color c)
{
	return color(
		color_srgb_to_scene_linear(c[0]),
		color_srgb_to_scene_linear(c[1]),
		color_srgb_to_scene_linear(c[2]));
}

color color_scene_linear_to_srgb(color c)
{
	return color(
		color_scene_linear_to_srgb(c[0]),
		color_scene_linear_to_srgb(c[1]),
		color_scene_linear_to_srgb(c[2]));
}

