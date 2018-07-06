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

CCL_NAMESPACE_BEGIN

/* Turbulence */

ccl_device_noinline float noise_turbulence(float3 p, float octaves, int hard)
{
	float fscale = 1.0f;
	float amp = 1.0f;
	float sum = 0.0f;
	int i, n;

	octaves = clamp(octaves, 0.0f, 16.0f);
	n = float_to_int(octaves);

	for(i = 0; i <= n; i++) {
		float t = noise(fscale*p);

		if(hard)
			t = fabsf(2.0f*t - 1.0f);

		sum += t*amp;
		amp *= 0.5f;
		fscale *= 2.0f;
	}

	float rmd = octaves - floorf(octaves);

	if(rmd != 0.0f) {
		float t = noise(fscale*p);

		if(hard)
			t = fabsf(2.0f*t - 1.0f);

		float sum2 = sum + t*amp;

		sum *= ((float)(1 << n)/(float)((1 << (n+1)) - 1));
		sum2 *= ((float)(1 << (n+1))/(float)((1 << (n+2)) - 1));

		return (1.0f - rmd)*sum + rmd*sum2;
	}
	else {
		sum *= ((float)(1 << n)/(float)((1 << (n+1)) - 1));
		return sum;
	}
}

CCL_NAMESPACE_END
