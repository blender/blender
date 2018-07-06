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

float fresnel_dielectric_cos(float cosi, float eta)
{
	/* compute fresnel reflectance without explicitly computing
	 * the refracted direction */
	float c = fabs(cosi);
	float g = eta * eta - 1 + c * c;
	float result;

	if (g > 0) {
		g = sqrt(g);
		float A = (g - c) / (g + c);
		float B = (c * (g + c) - 1) / (c * (g - c) + 1);
		result = 0.5 * A * A * (1 + B * B);
	}
	else
		result = 1.0;  /* TIR (no refracted component) */

	return result;
}

color fresnel_conductor(float cosi, color eta, color k)
{
	color cosi2 = color(cosi * cosi);
	color one = color(1, 1, 1);
	color tmp_f = eta * eta + k * k;
	color tmp = tmp_f * cosi2;
	color Rparl2 = (tmp - (2.0 * eta * cosi) + one) /
	               (tmp + (2.0 * eta * cosi) + one);
	color Rperp2 = (tmp_f - (2.0 * eta * cosi) + cosi2) /
	               (tmp_f + (2.0 * eta * cosi) + cosi2);
	return (Rparl2 + Rperp2) * 0.5;
}
