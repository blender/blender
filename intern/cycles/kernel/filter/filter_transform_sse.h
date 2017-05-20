/*
 * Copyright 2011-2017 Blender Foundation
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

ccl_device void kernel_filter_construct_transform(const float *ccl_restrict buffer,
                                                  int x, int y, int4 rect,
                                                  int pass_stride,
                                                  float *transform, int *rank,
                                                  int radius, float pca_threshold)
{
	int buffer_w = align_up(rect.z - rect.x, 4);

	__m128 features[DENOISE_FEATURES];
	const float *ccl_restrict pixel_buffer;
	int2 pixel;

	int2 low  = make_int2(max(rect.x, x - radius),
	                      max(rect.y, y - radius));
	int2 high = make_int2(min(rect.z, x + radius + 1),
	                      min(rect.w, y + radius + 1));
	int num_pixels = (high.y - low.y) * (high.x - low.x);

	__m128 feature_means[DENOISE_FEATURES];
	math_vector_zero_sse(feature_means, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW_SSE {
		filter_get_features_sse(x4, y4, active_pixels, pixel_buffer, features, NULL, pass_stride);
		math_vector_add_sse(feature_means, DENOISE_FEATURES, features);
	} END_FOR_PIXEL_WINDOW_SSE

	__m128 pixel_scale = _mm_set1_ps(1.0f / num_pixels);
	for(int i = 0; i < DENOISE_FEATURES; i++) {
		feature_means[i] = _mm_mul_ps(_mm_hsum_ps(feature_means[i]), pixel_scale);
	}

	__m128 feature_scale[DENOISE_FEATURES];
	math_vector_zero_sse(feature_scale, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW_SSE {
		filter_get_feature_scales_sse(x4, y4, active_pixels, pixel_buffer, features, feature_means, pass_stride);
		math_vector_max_sse(feature_scale, features, DENOISE_FEATURES);
	} END_FOR_PIXEL_WINDOW_SSE

	filter_calculate_scale_sse(feature_scale);

	__m128 feature_matrix_sse[DENOISE_FEATURES*DENOISE_FEATURES];
	math_matrix_zero_sse(feature_matrix_sse, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW_SSE {
		filter_get_features_sse(x4, y4, active_pixels, pixel_buffer, features, feature_means, pass_stride);
		math_vector_mul_sse(features, DENOISE_FEATURES, feature_scale);
		math_matrix_add_gramian_sse(feature_matrix_sse, DENOISE_FEATURES, features, _mm_set1_ps(1.0f));
	} END_FOR_PIXEL_WINDOW_SSE

	float feature_matrix[DENOISE_FEATURES*DENOISE_FEATURES];
	math_matrix_hsum(feature_matrix, DENOISE_FEATURES, feature_matrix_sse);

	math_matrix_jacobi_eigendecomposition(feature_matrix, transform, DENOISE_FEATURES, 1);

	*rank = 0;
	/* Prevent overfitting when a small window is used. */
	int max_rank = min(DENOISE_FEATURES, num_pixels/3);
	if(pca_threshold < 0.0f) {
		float threshold_energy = 0.0f;
		for(int i = 0; i < DENOISE_FEATURES; i++) {
			threshold_energy += feature_matrix[i*DENOISE_FEATURES+i];
		}
		threshold_energy *= 1.0f - (-pca_threshold);

		float reduced_energy = 0.0f;
		for(int i = 0; i < max_rank; i++, (*rank)++) {
			if(i >= 2 && reduced_energy >= threshold_energy)
				break;
			float s = feature_matrix[i*DENOISE_FEATURES+i];
			reduced_energy += s;
		}
	}
	else {
		for(int i = 0; i < max_rank; i++, (*rank)++) {
			float s = feature_matrix[i*DENOISE_FEATURES+i];
			if(i >= 2 && sqrtf(s) < pca_threshold)
				break;
		}
	}

	math_matrix_transpose(transform, DENOISE_FEATURES, 1);

	/* Bake the feature scaling into the transformation matrix. */
	for(int i = 0; i < DENOISE_FEATURES; i++) {
		math_vector_scale(transform + i*DENOISE_FEATURES, _mm_cvtss_f32(feature_scale[i]), *rank);
	}
}

CCL_NAMESPACE_END
