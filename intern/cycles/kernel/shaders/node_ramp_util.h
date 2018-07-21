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

/* NOTE: svm_ramp.h, svm_ramp_util.h and node_ramp_util.h must stay consistent */

color rgb_ramp_lookup(color ramp[], float at, int interpolate, int extrapolate)
{
	float f = at;
	int table_size = arraylength(ramp);

	if ((f < 0.0 || f > 1.0) && extrapolate) {
		color t0, dy;
		if (f < 0.0) {
			t0 = ramp[0];
			dy = t0 - ramp[1];
			f = -f;
		}
		else {
			t0 = ramp[table_size - 1];
			dy = t0 - ramp[table_size - 2];
			f = f - 1.0;
		}
		return t0 + dy * f * (table_size - 1);
	}

	f = clamp(at, 0.0, 1.0) * (table_size - 1);

	/* clamp int as well in case of NaN */
	int i = (int)f;
	if (i < 0) i = 0;
	if (i >= table_size) i = table_size - 1;
	float t = f - (float)i;

	color result = ramp[i];

	if (interpolate && t > 0.0)
		result = (1.0 - t) * result + t * ramp[i + 1];

	return result;
}

float rgb_ramp_lookup(float ramp[], float at, int interpolate, int extrapolate)
{
	float f = at;
	int table_size = arraylength(ramp);

	if ((f < 0.0 || f > 1.0) && extrapolate) {
		float t0, dy;
		if (f < 0.0) {
			t0 = ramp[0];
			dy = t0 - ramp[1];
			f = -f;
		}
		else {
			t0 = ramp[table_size - 1];
			dy = t0 - ramp[table_size - 2];
			f = f - 1.0;
		}
		return t0 + dy * f * (table_size - 1);
	}

	f = clamp(at, 0.0, 1.0) * (table_size - 1);

	/* clamp int as well in case of NaN */
	int i = (int)f;
	if (i < 0) i = 0;
	if (i >= table_size) i = table_size - 1;
	float t = f - (float)i;

	float result = ramp[i];

	if (interpolate && t > 0.0)
		result = (1.0 - t) * result + t * ramp[i + 1];

	return result;
}
