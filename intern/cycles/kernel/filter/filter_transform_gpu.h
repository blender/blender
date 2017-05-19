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

ccl_device void kernel_filter_construct_transform(const ccl_global float *ccl_restrict buffer,
                                                  int x, int y, int4 rect,
                                                  int pass_stride,
                                                  ccl_global float *transform,
                                                  ccl_global int *rank,
                                                  int radius, float pca_threshold,
                                                  int transform_stride, int localIdx)
{
	int buffer_w = align_up(rect.z - rect.x, 4);

#ifdef __KERNEL_CUDA__
	ccl_local float shared_features[DENOISE_FEATURES*CCL_MAX_LOCAL_SIZE];
	ccl_local_param float *features = shared_features + localIdx*DENOISE_FEATURES;
#else
	float features[DENOISE_FEATURES];
#endif

	/* === Calculate denoising window. === */
	int2 low  = make_int2(max(rect.x, x - radius),
	                      max(rect.y, y - radius));
	int2 high = make_int2(min(rect.z, x + radius + 1),
	                      min(rect.w, y + radius + 1));
	int num_pixels = (high.y - low.y) * (high.x - low.x);
	const ccl_global float *ccl_restrict pixel_buffer;
	int2 pixel;




	/* === Shift feature passes to have mean 0. === */
	float feature_means[DENOISE_FEATURES];
	math_vector_zero(feature_means, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW {
		filter_get_features(pixel, pixel_buffer, features, NULL, pass_stride);
		math_vector_add(feature_means, features, DENOISE_FEATURES);
	} END_FOR_PIXEL_WINDOW

	math_vector_scale(feature_means, 1.0f / num_pixels, DENOISE_FEATURES);

	/* === Scale the shifted feature passes to a range of [-1; 1], will be baked into the transform later. === */
	float feature_scale[DENOISE_FEATURES];
	math_vector_zero(feature_scale, DENOISE_FEATURES);

	FOR_PIXEL_WINDOW {
		filter_get_feature_scales(pixel, pixel_buffer, features, feature_means, pass_stride);
		math_vector_max(feature_scale, features, DENOISE_FEATURES);
	} END_FOR_PIXEL_WINDOW

	filter_calculate_scale(feature_scale);



	/* === Generate the feature transformation. ===
	 * This transformation maps the DENOISE_FEATURES-dimentional feature space to a reduced feature (r-feature) space
	 * which generally has fewer dimensions. This mainly helps to prevent overfitting. */
	float feature_matrix[DENOISE_FEATURES*DENOISE_FEATURES];
	math_matrix_zero(feature_matrix, DENOISE_FEATURES);
	FOR_PIXEL_WINDOW {
		filter_get_features(pixel, pixel_buffer, features, feature_means, pass_stride);
		math_vector_mul(features, feature_scale, DENOISE_FEATURES);
		math_matrix_add_gramian(feature_matrix, DENOISE_FEATURES, features, 1.0f);
	} END_FOR_PIXEL_WINDOW

	math_matrix_jacobi_eigendecomposition(feature_matrix, transform, DENOISE_FEATURES, transform_stride);
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

	math_matrix_transpose(transform, DENOISE_FEATURES, transform_stride);

	/* Bake the feature scaling into the transformation matrix. */
	for(int i = 0; i < DENOISE_FEATURES; i++) {
		for(int j = 0; j < (*rank); j++) {
			transform[(i*DENOISE_FEATURES + j)*transform_stride] *= feature_scale[i];
		}
	}
}

CCL_NAMESPACE_END
