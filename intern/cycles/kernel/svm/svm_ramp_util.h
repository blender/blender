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

#ifndef __SVM_RAMP_UTIL_H__
#define __SVM_RAMP_UTIL_H__

CCL_NAMESPACE_BEGIN

/* NOTE: svm_ramp.h, svm_ramp_util.h and node_ramp_util.h must stay consistent */

ccl_device_inline float3 rgb_ramp_lookup(const float3 *ramp,
                                         float f,
                                         bool interpolate,
                                         bool extrapolate,
                                         int table_size)
{
	if((f < 0.0f || f > 1.0f) && extrapolate) {
		float3 t0, dy;
		if(f < 0.0f) {
			t0 = ramp[0];
			dy = t0 - ramp[1],
			f = -f;
		}
		else {
			t0 = ramp[table_size - 1];
			dy = t0 - ramp[table_size - 2];
			f = f - 1.0f;
		}
		return t0 + dy * f * (table_size - 1);
	}

	f = clamp(f, 0.0f, 1.0f) * (table_size - 1);

	/* clamp int as well in case of NaN */
	int i = clamp(float_to_int(f), 0, table_size-1);
	float t = f - (float)i;

	float3 result = ramp[i];

	if(interpolate && t > 0.0f) {
		result = (1.0f - t) * result + t * ramp[i + 1];
	}

	return result;
}

ccl_device float float_ramp_lookup(const float *ramp,
                                   float f,
                                   bool interpolate,
                                   bool extrapolate,
                                   int table_size)
{
	if((f < 0.0f || f > 1.0f) && extrapolate) {
		float t0, dy;
		if(f < 0.0f) {
			t0 = ramp[0];
			dy = t0 - ramp[1],
			f = -f;
		}
		else {
			t0 = ramp[table_size - 1];
			dy = t0 - ramp[table_size - 2];
			f = f - 1.0f;
		}
		return t0 + dy * f * (table_size - 1);
	}

	f = clamp(f, 0.0f, 1.0f) * (table_size - 1);

	/* clamp int as well in case of NaN */
	int i = clamp(float_to_int(f), 0, table_size-1);
	float t = f - (float)i;

	float result = ramp[i];

	if(interpolate && t > 0.0f) {
		result = (1.0f - t) * result + t * ramp[i + 1];
	}

	return result;
}

CCL_NAMESPACE_END

#endif /* __SVM_RAMP_UTIL_H__ */
