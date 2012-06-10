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
	if (c < 0.04045)
		return (c < 0.0) ? 0.0 : c * (1.0 / 12.92);
	else
		return pow((c + 0.055) * (1.0 / 1.055), 2.4);
}

float color_scene_linear_to_srgb(float c)
{
	if (c < 0.0031308)
		return (c < 0.0) ? 0.0 : c * 12.92;
	else
		return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
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

/* Color Operations */

color rgb_to_hsv(color rgb)
{
	float cmax, cmin, h, s, v, cdelta;
	color c;

	cmax = max(rgb[0], max(rgb[1], rgb[2]));
	cmin = min(rgb[0], min(rgb[1], rgb[2]));
	cdelta = cmax - cmin;

	v = cmax;

	if (cmax != 0.0) {
		s = cdelta / cmax;
	}
	else {
		s = 0.0;
		h = 0.0;
	}

	if (s == 0.0) {
		h = 0.0;
	}
	else {
		c = (color(cmax, cmax, cmax) - rgb) / cdelta;

		if (rgb[0] == cmax) h = c[2] - c[1];
		else if (rgb[1] == cmax) h = 2.0 + c[0] -  c[2];
		else h = 4.0 + c[1] - c[0];

		h /= 6.0;

		if (h < 0.0)
			h += 1.0;
	}

	return color(h, s, v);
}

color hsv_to_rgb(color hsv)
{
	float i, f, p, q, t, h, s, v;
	color rgb;

	h = hsv[0];
	s = hsv[1];
	v = hsv[2];

	if (s == 0.0) {
		rgb = color(v, v, v);
	}
	else {
		if (h == 1.0)
			h = 0.0;
		
		h *= 6.0;
		i = floor(h);
		f = h - i;
		rgb = color(f, f, f);
		p = v * (1.0 - s);
		q = v * (1.0 - (s * f));
		t = v * (1.0 - (s * (1.0 - f)));

		if (i == 0.0) rgb = color(v, t, p);
		else if (i == 1.0) rgb = color(q, v, p);
		else if (i == 2.0) rgb = color(p, v, t);
		else if (i == 3.0) rgb = color(p, q, v);
		else if (i == 4.0) rgb = color(t, p, v);
		else rgb = color(v, p, q);
	}

	return rgb;
}

