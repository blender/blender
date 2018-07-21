/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* TODO(lukas): Fix colors in OSL. */

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

color color_unpremultiply(color c, float alpha)
{
	if (alpha != 1.0 && alpha != 0.0)
		return c / alpha;

	return c;
}

/* Color Operations */

color xyY_to_xyz(float x, float y, float Y)
{
	float X, Z;

	if (y != 0.0) X = (x / y) * Y;
	else X = 0.0;

	if (y != 0.0 && Y != 0.0) Z = ((1.0 - x - y) / y) * Y;
	else Z = 0.0;

	return color(X, Y, Z);
}

color xyz_to_rgb(float x, float y, float z)
{
	return color( 3.240479 * x + -1.537150 * y + -0.498535 * z,
	             -0.969256 * x +  1.875991 * y +  0.041556 * z,
	              0.055648 * x + -0.204043 * y +  1.057311 * z);
}

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
