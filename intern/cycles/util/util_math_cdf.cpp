/*
 * Copyright 2011-2015 Blender Foundation
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

#include "util/util_math_cdf.h"

#include "util/util_algorithm.h"
#include "util/util_math.h"

CCL_NAMESPACE_BEGIN

/* Invert pre-calculated CDF function. */
void util_cdf_invert(const int resolution,
                     const float from,
                     const float to,
                     const vector<float> &cdf,
                     const bool make_symmetric,
                     vector<float> &inv_cdf) {
	const float inv_resolution = 1.0f / (float)resolution;
	const float range = to - from;
	inv_cdf.resize(resolution);
	if(make_symmetric) {
		const int half_size = (resolution - 1) / 2;
		for(int i = 0; i <= half_size; i++) {
			float x = i / (float)half_size;
			int index = upper_bound(cdf.begin(), cdf.end(), x) - cdf.begin();
			float t;
			if(index < cdf.size() - 1) {
				t = (x - cdf[index])/(cdf[index+1] - cdf[index]);
			} else {
				t = 0.0f;
				index = cdf.size() - 1;
			}
			float y = ((index + t) / (resolution - 1)) * (2.0f * range);
			inv_cdf[half_size+i] = 0.5f*(1.0f + y);
			inv_cdf[half_size-i] = 0.5f*(1.0f - y);
		}
	}
	else {
		for(int i = 0; i < resolution; i++) {
			float x = from + range * (float)i * inv_resolution;
			int index = upper_bound(cdf.begin(), cdf.end(), x) - cdf.begin();
			float t;
			if(index < cdf.size() - 1) {
				t = (x - cdf[index])/(cdf[index+1] - cdf[index]);
			} else {
				t = 0.0f;
				index = resolution;
			}
			inv_cdf[i] = (index + t) * inv_resolution;
		}
	}
}

CCL_NAMESPACE_END
