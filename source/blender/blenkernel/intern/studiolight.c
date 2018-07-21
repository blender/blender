/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is Copyright (C) 2006-2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenkernel/intern/studiolight.c
 *  \ingroup bke
 */

#include "BKE_studiolight.h"

#include "BKE_appdir.h"
#include "BKE_icons.h"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_path_util.h"
#include "BLI_rand.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "DNA_listBase.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "GPU_texture.h"

#include "MEM_guardedalloc.h"


/* Statics */
static ListBase studiolights;
static int last_studiolight_id = 0;
#define STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE 128
#define STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_HEIGHT 32
#define STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_WIDTH (STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_HEIGHT * 2)

#define STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE 0
#define STUDIOLIGHT_IRRADIANCE_METHOD_SPHERICAL_HARMONICS 1
/*
 * The method to calculate the irradiance buffers
 * The irradiance buffer is only shown in the background when in LookDev.
 *
 * STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE is very slow, but very accurate
 * STUDIOLIGHT_IRRADIANCE_METHOD_SPHERICAL_HARMONICS is faster but has artifacts
 */
// #define STUDIOLIGHT_IRRADIANCE_METHOD STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
#define STUDIOLIGHT_IRRADIANCE_METHOD STUDIOLIGHT_IRRADIANCE_METHOD_SPHERICAL_HARMONICS

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL == 2
#  define STUDIOLIGHT_SPHERICAL_HARMONICS_WINDOWING
#endif

/*
 * Disable this option so caches are not loaded from disk
 * Do not checkin with this commented out
 */
#define STUDIOLIGHT_LOAD_CACHED_FILES

static const char *STUDIOLIGHT_CAMERA_FOLDER = "studiolights/camera/";
static const char *STUDIOLIGHT_WORLD_FOLDER = "studiolights/world/";
static const char *STUDIOLIGHT_MATCAP_FOLDER = "studiolights/matcap/";

/* FUNCTIONS */
#define IMB_SAFE_FREE(p) do { \
if (p) {                      \
	IMB_freeImBuf(p);         \
	p = NULL;                 \
}                             \
} while (0)

#define GPU_TEXTURE_SAFE_FREE(p) do { \
if (p) {                              \
	GPU_texture_free(p);              \
	p = NULL;                         \
}                                     \
} while (0)

static void studiolight_free(struct StudioLight *sl)
{
#define STUDIOLIGHT_DELETE_ICON(s) {  \
	if (s != 0) {                          \
		BKE_icon_delete(s);           \
		s = 0;                        \
	}                                 \
}
	if (sl->free_function) {
		sl->free_function(sl, sl->free_function_data);
	}
	STUDIOLIGHT_DELETE_ICON(sl->icon_id_radiance);
	STUDIOLIGHT_DELETE_ICON(sl->icon_id_irradiance);
	STUDIOLIGHT_DELETE_ICON(sl->icon_id_matcap);
	STUDIOLIGHT_DELETE_ICON(sl->icon_id_matcap_flipped);
#undef STUDIOLIGHT_DELETE_ICON

	for (int index = 0; index < 6; index++) {
		IMB_SAFE_FREE(sl->radiance_cubemap_buffers[index]);
	}
	GPU_TEXTURE_SAFE_FREE(sl->equirectangular_radiance_gputexture);
	GPU_TEXTURE_SAFE_FREE(sl->equirectangular_irradiance_gputexture);
	IMB_SAFE_FREE(sl->equirectangular_radiance_buffer);
	IMB_SAFE_FREE(sl->equirectangular_irradiance_buffer);
	MEM_SAFE_FREE(sl->path_irr_cache);
	MEM_SAFE_FREE(sl->path_sh_cache);
	MEM_SAFE_FREE(sl->gpu_matcap_3components);
	MEM_SAFE_FREE(sl);
}

static struct StudioLight *studiolight_create(int flag)
{
	struct StudioLight *sl = MEM_callocN(sizeof(*sl), __func__);
	sl->path[0] = 0x00;
	sl->name[0] = 0x00;
	sl->path_irr_cache = NULL;
	sl->path_sh_cache = NULL;
	sl->free_function = NULL;
	sl->flag = flag;
	sl->index = ++last_studiolight_id;
	if (flag & STUDIOLIGHT_ORIENTATION_VIEWNORMAL) {
		sl->icon_id_matcap = BKE_icon_ensure_studio_light(sl, STUDIOLIGHT_ICON_ID_TYPE_MATCAP);
		sl->icon_id_matcap_flipped = BKE_icon_ensure_studio_light(sl, STUDIOLIGHT_ICON_ID_TYPE_MATCAP_FLIPPED);
	}
	else {
		sl->icon_id_radiance = BKE_icon_ensure_studio_light(sl, STUDIOLIGHT_ICON_ID_TYPE_RADIANCE);
		sl->icon_id_irradiance = BKE_icon_ensure_studio_light(sl, STUDIOLIGHT_ICON_ID_TYPE_IRRADIANCE);
	}

	for (int index = 0; index < 6; index++) {
		sl->radiance_cubemap_buffers[index] = NULL;
	}

	return sl;
}

static void direction_to_equirectangular(float r[2], const float dir[3])
{
	r[0] = (atan2f(dir[1], dir[0]) - M_PI) / -(M_PI * 2);
	r[1] = (acosf(dir[2] / 1.0) - M_PI) / -M_PI;
}

static void equirectangular_to_direction(float r[3], float u, float v)
{
	float phi = (-(M_PI * 2)) * u + M_PI;
	float theta = -M_PI * v + M_PI;
	float sin_theta = sinf(theta);
	r[0] = sin_theta * cosf(phi);
	r[1] = sin_theta * sinf(phi);
	r[2] = cosf(theta);
}

static void studiolight_calculate_radiance(ImBuf *ibuf, float color[4], const float direction[3])
{
	float uv[2];
	direction_to_equirectangular(uv, direction);
	nearest_interpolation_color_wrap(ibuf, NULL, color, uv[0] * ibuf->x, uv[1] * ibuf->y);
}

static void studiolight_calculate_radiance_buffer(
        ImBuf *ibuf, float *colbuf,
        const float start_x, const float add_x,
        const float start_y, const float add_y, const float z,
        const int index_x, const int index_y, const int index_z)
{
	float direction[3];
	float yf = start_y;
	float xf;
	float *color = colbuf;

	for (int y = 0; y < STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE; y++, yf += add_y) {
		xf = start_x;
		for (int x = 0; x < STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE; x++, xf += add_x) {
			direction[index_x] = xf;
			direction[index_y] = yf;
			direction[index_z] = z;
			normalize_v3(direction);
			studiolight_calculate_radiance(ibuf, color, direction);
			color += 4;
		}
	}
}

static void studiolight_load_equirectangular_image(StudioLight *sl)
{
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
		ImBuf *ibuf = NULL;
		ibuf = IMB_loadiffname(sl->path, 0, NULL);
		if (ibuf == NULL) {
			float *colbuf = MEM_mallocN(sizeof(float[4]), __func__);
			copy_v4_fl4(colbuf, 1.0f, 0.0f, 1.0f, 1.0f);
			ibuf = IMB_allocFromBuffer(NULL, colbuf, 1, 1);
		}
		IMB_float_from_rect(ibuf);
		sl->equirectangular_radiance_buffer = ibuf;
	}
	sl->flag |= STUDIOLIGHT_EXTERNAL_IMAGE_LOADED;
}

static void studiolight_create_equirectangular_radiance_gputexture(StudioLight *sl)
{
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
		char error[256];
		BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);
		ImBuf *ibuf = sl->equirectangular_radiance_buffer;

		if (sl->flag & STUDIOLIGHT_ORIENTATION_VIEWNORMAL) {
			sl->gpu_matcap_3components = MEM_callocN(sizeof(float[3]) * ibuf->x * ibuf->y, __func__);

			float *offset4 = ibuf->rect_float;
			float *offset3 = sl->gpu_matcap_3components;
			for (int i = 0; i < ibuf->x * ibuf->y; i++) {
				copy_v3_v3(offset3, offset4);
				offset3 += 3;
				offset4 += 4;
			}
			sl->equirectangular_radiance_gputexture = GPU_texture_create_nD(
			        ibuf->x, ibuf->y, 0, 2, sl->gpu_matcap_3components, GPU_R11F_G11F_B10F, GPU_DATA_FLOAT, 0, false, error);
		}
		else {
			sl->equirectangular_radiance_gputexture = GPU_texture_create_2D(
			        ibuf->x, ibuf->y, GPU_RGBA16F, ibuf->rect_float, error);
			GPUTexture *tex = sl->equirectangular_radiance_gputexture;
			GPU_texture_bind(tex, 0);
			GPU_texture_filter_mode(tex, true);
			GPU_texture_wrap_mode(tex, true);
			GPU_texture_unbind(tex);
		}
	}
	sl->flag |= STUDIOLIGHT_EQUIRECTANGULAR_RADIANCE_GPUTEXTURE;
}

static void studiolight_create_equirectangular_irradiance_gputexture(StudioLight *sl)
{
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
		char error[256];
		BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_IMAGE_CALCULATED);
		ImBuf *ibuf = sl->equirectangular_irradiance_buffer;
		sl->equirectangular_irradiance_gputexture = GPU_texture_create_2D(
		        ibuf->x, ibuf->y, GPU_RGBA16F, ibuf->rect_float, error);
		GPUTexture *tex = sl->equirectangular_irradiance_gputexture;
		GPU_texture_bind(tex, 0);
		GPU_texture_filter_mode(tex, true);
		GPU_texture_wrap_mode(tex, true);
		GPU_texture_unbind(tex);
	}
	sl->flag |= STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_GPUTEXTURE;
}

static void studiolight_calculate_radiance_cubemap_buffers(StudioLight *sl)
{
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
		BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);
		ImBuf *ibuf = sl->equirectangular_radiance_buffer;
		if (ibuf) {
			float *colbuf = MEM_mallocN(SQUARE(STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) * sizeof(float[4]), __func__);
			const float add = 1.0f / (STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE + 1);
			const float start = ((1.0f / STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) * 0.5f) - 0.5f;

			/* front */
			studiolight_calculate_radiance_buffer(ibuf, colbuf, start, add, start, add, 0.5f, 0, 2, 1);
			sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_POS] = IMB_allocFromBuffer(
			        NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

			/* back */
			studiolight_calculate_radiance_buffer(ibuf, colbuf, -start, -add, start, add, -0.5f, 0, 2, 1);
			sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_NEG] = IMB_allocFromBuffer(
			        NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

			/* left */
			studiolight_calculate_radiance_buffer(ibuf, colbuf, -start, -add, start, add, 0.5f, 1, 2, 0);
			sl->radiance_cubemap_buffers[STUDIOLIGHT_X_POS] = IMB_allocFromBuffer(
			        NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

			/* right */
			studiolight_calculate_radiance_buffer(ibuf, colbuf, start, add, start, add, -0.5f, 1, 2, 0);
			sl->radiance_cubemap_buffers[STUDIOLIGHT_X_NEG] = IMB_allocFromBuffer(
			        NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

			/* top */
			studiolight_calculate_radiance_buffer(ibuf, colbuf, start, add, start, add, -0.5f, 0, 1, 2);
			sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_NEG] = IMB_allocFromBuffer(
			        NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

			/* bottom */
			studiolight_calculate_radiance_buffer(ibuf, colbuf, start, add, -start, -add, 0.5f, 0, 1, 2);
			sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_POS] = IMB_allocFromBuffer(
			        NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

#if 0
			IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_X_POS], "/tmp/studiolight_radiance_left.png", IB_rectfloat);
			IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_X_NEG], "/tmp/studiolight_radiance_right.png", IB_rectfloat);
			IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_POS], "/tmp/studiolight_radiance_front.png", IB_rectfloat);
			IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_NEG], "/tmp/studiolight_radiance_back.png", IB_rectfloat);
			IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_POS], "/tmp/studiolight_radiance_bottom.png", IB_rectfloat);
			IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_NEG], "/tmp/studiolight_radiance_top.png", IB_rectfloat);
#endif
			MEM_freeN(colbuf);
		}
	}
	sl->flag |= STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED;
}

BLI_INLINE void studiolight_evaluate_radiance_buffer(
        ImBuf *radiance_buffer, const float normal[3], float color[3], int *hits,
        int xoffset, int yoffset, int zoffset, float zvalue)
{
	if (radiance_buffer == NULL) {
		return;
	}
	float angle;
	float *radiance_color = radiance_buffer->rect_float;
	float direction[3];
	for (int y = 0; y < STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE; y++) {
		for (int x = 0; x < STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE; x++) {
			// calculate light direction;
			direction[zoffset] = zvalue;
			direction[xoffset] = (x / (float)STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) - 0.5f;
			direction[yoffset] = (y / (float)STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) - 0.5f;
			normalize_v3(direction);
			angle = fmax(0.0f, dot_v3v3(direction, normal));
			madd_v3_v3fl(color, radiance_color, angle);
			(*hits)++;
			radiance_color += 4;
		}
	}

}

/*
 * Spherical Harmonics
 */
BLI_INLINE float studiolight_area_element(float x, float y)
{
	return atan2(x * y, sqrtf(x * x + y * y + 1));
}

BLI_INLINE float studiolight_texel_solid_angle(float x, float y, float halfpix)
{
	float v1x = (x - halfpix) * 2.0f - 1.0f;
	float v1y = (y - halfpix) * 2.0f - 1.0f;
	float v2x = (x + halfpix) * 2.0f - 1.0f;
	float v2y = (y + halfpix) * 2.0f - 1.0f;

	return studiolight_area_element(v1x, v1y) - studiolight_area_element(v1x, v2y) - studiolight_area_element(v2x, v1y) + studiolight_area_element(v2x, v2y);
}

static void studiolight_calculate_cubemap_vector_weight(float normal[3], float *weight, int face, float x, float y)
{
	copy_v3_fl3(normal, x * 2.0f - 1.0f, y * 2.0f - 1.0f, 1.0f);
	const float conversion_matrices[6][3][3] = {
		{
			{0.0f,  0.0f, 1.0f},
			{0.0f, -1.0f, 0.0f},
			{1.0f,  0.0f, 0.0f},
		},
		{
			{0.0f, 0.0f, -1.0f},
			{0.0f, -1.0f, 0.0f},
			{-1.0f, 0.0f, 0.0f},
		},
		{
			{1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, -1.0f},
			{0.0f, 1.0f, 0.0f},
		},
		{
			{1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f},
			{0.0f, -1.0f, 0.0f},
		},
		{
			{1.0f, 0.0f, 0.0f},
			{0.0f, -1.0f, 0.0f},
			{0.0f, 0.0f, -1.0f},
		},
		{
			{-1.0f, 0.0f, 0.0f},
			{0.0f, -1.0f, 0.0f},
			{0.0f, 0.0f, 1.0f},
		}
	};

	mul_m3_v3(conversion_matrices[face], normal);
	normalize_v3(normal);
	const float halfpix = 1.0f / (2.0f * STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);
	*weight = studiolight_texel_solid_angle(x + halfpix, y + halfpix, halfpix);
}

static void studiolight_calculate_spherical_harmonics_coefficient(StudioLight *sl, int sh_component)
{
	const float M_4PI = M_PI * 4.0f;

	float weight_accum = 0.0f;
	float sh[3] = {0.0f, 0.0f, 0.0f};
	for (int face = 0; face < 6; face++) {
		float *color;
		color = sl->radiance_cubemap_buffers[face]->rect_float;
		for (int y = 0; y < STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE; y++) {
			float yf = y / (float)STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE;
			for (int x = 0; x < STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE; x++) {
				float xf = x / (float)STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE;
				float weight, coef;
				float cubevec[3];
				studiolight_calculate_cubemap_vector_weight(cubevec, &weight, face, xf, yf);

				const float nx = cubevec[0];
				const float ny = cubevec[1];
				const float nz = cubevec[2];
				const float nx2 = SQUARE(nx);
				const float ny2 = SQUARE(ny);
				const float nz2 = SQUARE(nz);
				const float nx4 = SQUARE(nx2);
				const float ny4 = SQUARE(ny2);
				const float nz4 = SQUARE(nz2);

				switch (sh_component) {
					/* L0 */
					case 0:
						coef = 0.2822095f;
						break;
					/* L1 */
					case 1:
						coef = -0.488603f * nz * 2.0f / 3.0f;
						break;
					case 2:
						coef = 0.488603f * ny * 2.0f / 3.0f;
						break;
					case 3:
						coef = -0.488603f * nx * 2.0f / 3.0f;
						break;
					/* L2 */
					case 4:
						coef = 1.092548f * nx * nz * 1.0f / 4.0f;
						break;
					case 5:
						coef = -1.092548f * nz * ny * 1.0f / 4.0f;
						break;
					case 6:
						coef = 0.315392f * (3.0f * ny2 - 1.0f) * 1.0f / 4.0f;
						break;
					case 7:
						coef = 1.092548f * nx * ny * 1.0f / 4.0f;
						break;
					case 8:
						coef = 0.546274f * (nx2 - nz2) * 1.0f / 4.0f;
						break;
					/* L4 */
					case 9:
						coef = (2.5033429417967046f * nx * nz * (nx2 - nz2)) / -24.0f;
						break;
					case 10:
						coef = (-1.7701307697799304f * nz * ny * (3.0f * nx2 - nz2)) / -24.0f;
						break;
					case 11:
						coef = (0.9461746957575601f * nz * nx * (-1.0f + 7.0f * ny2)) / -24.0f;
						break;
					case 12:
						coef = (-0.6690465435572892f * nz * ny * (-3.0f + 7.0f * ny2)) / -24.0f;
						break;
					case 13:
						coef = ((105.0f * ny4 - 90.0f * ny2 + 9.0f) / 28.359261614f) / -24.0f;
						break;
					case 14:
						coef = (-0.6690465435572892f * nx * ny * (-3.0f + 7.0f * ny2)) / -24.0f;
						break;
					case 15:
						coef = (0.9461746957575601f * (nx2 - nz2) * (-1.0f + 7.0f * ny2)) / -24.0f;
						break;
					case 16:
						coef = (-1.7701307697799304f * nx * ny * (nx2 - 3.0f * nz2)) / -24.0f;
						break;
					case 17:
						coef = (0.6258357354491761f * (nx4 - 6.0f * nz2 * nx2 + nz4)) / -24.0f;
						break;
					default:
						coef = 0.0f;
				}

				madd_v3_v3fl(sh, color, coef * weight);
				weight_accum += weight;
				color += 4;
			}
		}
	}

	mul_v3_fl(sh, M_4PI / weight_accum);
	copy_v3_v3(sl->spherical_harmonics_coefs[sh_component], sh);
}

#ifdef STUDIOLIGHT_SPHERICAL_HARMONICS_WINDOWING
static void studiolight_calculate_spherical_harmonics_luminance(StudioLight *sl, float luminance[STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS])
{
	for (int index = 0; index < STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS; index++) {
		luminance[index] = rgb_to_grayscale(sl->spherical_harmonics_coefs[index]);
	}
}

static void studiolight_apply_spherical_harmonics_windowing(StudioLight *sl, float max_lamplacian)
{
	/* From Peter-Pike Sloan's Stupid SH Tricks http://www.ppsloan.org/publications/StupidSH36.pdf */
	float table_l[STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL + 1];
	float table_b[STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL + 1];

	table_l[0] = 0.0f;
	table_b[0] = 0.0f;

	/* convert to luminance */
	float luminance[STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS];
	studiolight_calculate_spherical_harmonics_luminance(sl, luminance);

	int index = 1;
	for (int level = 1; level <= STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL; level++) {
		table_l[level] = (float)(SQUARE(level) * SQUARE(level + 1));

		float b = 0.0f;
		for (int m = -1; m <= level; m++) {
			b += SQUARE(luminance[index++]);
		}
		table_b[level] = b;
	}

	float squared_lamplacian = 0.0f;
	for (int level = 1; level <= STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL; level++) {
		squared_lamplacian += table_l[level] * table_b[level];
	}

	const float target_squared_laplacian = max_lamplacian * max_lamplacian;
	if (squared_lamplacian <= target_squared_laplacian) {
		return;
	}

	float lambda = 0.0f;

	const int no_iterations = 10000000;
	for (int i = 0; i < no_iterations; ++i) {
		float f = 0.0f;
		float fd = 0.0f;

		for (int level = 1; level <= (int)STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL; ++level) {
			f += table_l[level] * table_b[level] / SQUARE(1.0f + lambda * table_l[level]);
			fd += (2.0f * SQUARE(table_l[level]) * table_b[level]) / CUBE(1.0f + lambda * table_l[level]);
		}

		f = target_squared_laplacian - f;

		float delta = -f / fd;
		lambda += delta;

		if (ABS(delta) < 1e-6f) {
			break;
		}
	}

	/* Apply windowing lambda */
	index = 0;
	for (int level = 0; level <= STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL; level++) {
		float s = 1.0f / (1.0f + lambda * SQUARE(level) * SQUARE(level + 1.0f));

		for (int m = -1; m <= level; m++) {
			mul_v3_fl(sl->spherical_harmonics_coefs[index++], s);
		}
	}
}
#endif

BLI_INLINE void studiolight_sample_spherical_harmonics(StudioLight *sl, float color[3], float normal[3])
{
	const float nx = normal[0];
	const float ny = normal[1];
	const float nz = normal[2];

	copy_v3_fl(color, 0.0f);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[0], 0.282095f);

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL > 0
	/* Spherical Harmonics L1 */
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[1], -0.488603f * nz);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[2],  0.488603f * ny);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[3], -0.488603f * nx);
#endif

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL > 1
	/* Spherical Harmonics L2 */
	const float nx2 = SQUARE(nx);
	const float ny2 = SQUARE(ny);
	const float nz2 = SQUARE(nz);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[4], 1.092548f * nx * nz);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[5], -1.092548f * nz * ny);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[6], 0.315392f * (3.0f * ny2 - 1.0f));
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[7], -1.092548 * nx * ny);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[8], 0.546274 * (nx2 - nz2));
#endif

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL > 3
	/* Spherical Harmonics L4 */
	const float nx4 = SQUARE(nx2);
	const float ny4 = SQUARE(ny2);
	const float nz4 = SQUARE(nz2);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[9],   2.5033429417967046f * nx * nz * (nx2 - nz2));
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[10], -1.7701307697799304f * nz * ny * (3.0f * nx2 - nz2));
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[11],  0.9461746957575601f * nz * nx * (-1.0f + 7.0f * ny2));
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[12], -0.6690465435572892f * nz * ny * (-3.0f + 7.0f * ny2));
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[13],  (105.0f * ny4 - 90.0f * ny2 + 9.0f) / 28.359261614f);
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[14], -0.6690465435572892f * nx * ny * (-3.0f + 7.0f * ny2));
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[15],  0.9461746957575601f * (nx2 - nz2) * (-1.0f + 7.0f * ny2));
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[16], -1.7701307697799304f * nx * ny * (nx2 - 3.0f * nz2));
	madd_v3_v3fl(color, sl->spherical_harmonics_coefs[17],  0.6258357354491761f * (nx4 - 6.0f * nz2 * nx2 + nz4));
#endif

}

static void studiolight_calculate_diffuse_light(StudioLight *sl)
{
	/* init light to black */
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
		BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED);

		for (int comp = 0; comp < STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS; comp++) {
			studiolight_calculate_spherical_harmonics_coefficient(sl, comp);
		}

#ifdef STUDIOLIGHT_SPHERICAL_HARMONICS_WINDOWING
		studiolight_apply_spherical_harmonics_windowing(sl, STUDIOLIGHT_SPHERICAL_HARMONICS_WINDOWING_TARGET_LAMPLACIAN);
#endif

		if (sl->flag & STUDIOLIGHT_USER_DEFINED) {
			FILE *fp = BLI_fopen(sl->path_sh_cache, "wb");
			if (fp) {
				fwrite(sl->spherical_harmonics_coefs, sizeof(sl->spherical_harmonics_coefs), 1, fp);
				fclose(fp);
			}
		}
	}
	sl->flag |= STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED;
}

static float texel_coord_solid_angle(float a_U, float a_V, int a_Size)
{
	//scale up to [-1, 1] range (inclusive), offset by 0.5 to point to texel center.
	float u = (2.0f * ((float)a_U + 0.5f) / (float)a_Size) - 1.0f;
	float v = (2.0f * ((float)a_V + 0.5f) / (float)a_Size) - 1.0f;

	float resolution_inv = 1.0f / a_Size;

	// U and V are the -1..1 texture coordinate on the current face.
	// Get projected area for this texel
	float x0 = u - resolution_inv;
	float y0 = v - resolution_inv;
	float x1 = u + resolution_inv;
	float y1 = v + resolution_inv;
	return studiolight_area_element(x0, y0) - studiolight_area_element(x0, y1) - studiolight_area_element(x1, y0) + studiolight_area_element(x1, y1);
}

BLI_INLINE void studiolight_evaluate_specular_radiance_buffer(
        ImBuf *radiance_buffer, const float normal[3], float color[3],
        int xoffset, int yoffset, int zoffset, float zvalue)
{
	if (radiance_buffer == NULL) {
		return;
	}
	float angle;
	float *radiance_color = radiance_buffer->rect_float;
	float direction[3];
	for (int y = 0; y < STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE; y++) {
		for (int x = 0; x < STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE; x++) {
			// calculate light direction;
			float u = (x / (float)STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) - 0.5f;
			float v = (y / (float)STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) - 0.5f;
			direction[zoffset] = zvalue;
			direction[xoffset] = u;
			direction[yoffset] = v;
			normalize_v3(direction);
			angle = fmax(0.0f, dot_v3v3(direction, normal)) * texel_coord_solid_angle(x, y, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);
			madd_v3_v3fl(color, radiance_color, angle);
			radiance_color += 4;
		}
	}

}

#if STUDIOLIGHT_IRRADIANCE_METHOD == STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
static void studiolight_calculate_specular_irradiance(StudioLight *sl, float color[3], const float normal[3])
{
	copy_v3_fl(color, 0.0f);

	/* back */
	studiolight_evaluate_specular_radiance_buffer(
	        sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_POS], normal, color, 0, 2, 1, 0.5);
	/* front */
	studiolight_evaluate_specular_radiance_buffer(
	        sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_NEG], normal, color, 0, 2, 1, -0.5);

	/* left */
	studiolight_evaluate_specular_radiance_buffer(
	        sl->radiance_cubemap_buffers[STUDIOLIGHT_X_POS], normal, color, 1, 2, 0, 0.5);
	/* right */
	studiolight_evaluate_specular_radiance_buffer(
	        sl->radiance_cubemap_buffers[STUDIOLIGHT_X_NEG], normal, color, 1, 2, 0, -0.5);

	/* top */
	studiolight_evaluate_specular_radiance_buffer(
	        sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_POS], normal, color, 0, 1, 2, 0.5);
	/* bottom */
	studiolight_evaluate_specular_radiance_buffer(
	        sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_NEG], normal, color, 0, 1, 2, -0.5);

	mul_v3_fl(color, 1.0 / M_PI);
}
#endif

static bool studiolight_load_irradiance_equirectangular_image(StudioLight *sl)
{
#ifdef STUDIOLIGHT_LOAD_CACHED_FILES
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
		ImBuf *ibuf = NULL;
		ibuf = IMB_loadiffname(sl->path_irr_cache, 0, NULL);
		if (ibuf) {
			IMB_float_from_rect(ibuf);
			sl->equirectangular_irradiance_buffer = ibuf;
			sl->flag |= STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_IMAGE_CALCULATED;
			return true;
		}
	}
#endif
	return false;
}

static bool studiolight_load_spherical_harmonics_coefficients(StudioLight *sl)
{
#ifdef STUDIOLIGHT_LOAD_CACHED_FILES
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
		FILE *fp = BLI_fopen(sl->path_sh_cache, "rb");
		if (fp) {
			if (fread((void *)(sl->spherical_harmonics_coefs), sizeof(sl->spherical_harmonics_coefs), 1, fp)) {
				sl->flag |= STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED;
				fclose(fp);
				return true;
			}
			fclose(fp);
		}
	}
#endif
	return false;
}

static void studiolight_calculate_irradiance_equirectangular_image(StudioLight *sl)
{
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
#if STUDIOLIGHT_IRRADIANCE_METHOD == STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
		BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED);
#endif
#if STUDIOLIGHT_IRRADIANCE_METHOD == STUDIOLIGHT_IRRADIANCE_METHOD_SPHERICAL_HARMONICS
		BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED);
#endif

		float *colbuf = MEM_mallocN(STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_WIDTH * STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_HEIGHT * sizeof(float[4]), __func__);
		float *color = colbuf;
		for (int y = 0; y < STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_HEIGHT; y++) {
			float yf = y / (float)STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_HEIGHT;

			for (int x = 0; x < STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_WIDTH; x++) {
				float xf = x / (float)STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_WIDTH;
				float dir[3];
				equirectangular_to_direction(dir, xf, yf);

#if STUDIOLIGHT_IRRADIANCE_METHOD == STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
				studiolight_calculate_specular_irradiance(sl, color, dir);
#endif
#if STUDIOLIGHT_IRRADIANCE_METHOD == STUDIOLIGHT_IRRADIANCE_METHOD_SPHERICAL_HARMONICS
				studiolight_sample_spherical_harmonics(sl, color, dir);
#endif

				color[3] = 1.0f;
				color += 4;
			}
		}

		sl->equirectangular_irradiance_buffer = IMB_allocFromBuffer(
		        NULL, colbuf,
		        STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_WIDTH,
		        STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_HEIGHT);
		MEM_freeN(colbuf);

#if STUDIOLIGHT_IRRADIANCE_METHOD == STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
		/*
		 * Only store cached files when using STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
		 */
		if (sl->flag & STUDIOLIGHT_USER_DEFINED) {
			IMB_saveiff(sl->equirectangular_irradiance_buffer, sl->path_irr_cache, IB_rectfloat);
		}
#endif
	}
	sl->flag |= STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_IMAGE_CALCULATED;
}

static void studiolight_calculate_light_direction(StudioLight *sl)
{
	float best_light = 0.0;
	sl->light_direction[0] = 0.0f;
	sl->light_direction[1] = 0.0f;
	sl->light_direction[2] = -1.0f;

	if ((sl->flag & STUDIOLIGHT_EXTERNAL_FILE) && (sl->flag & STUDIOLIGHT_ORIENTATION_WORLD)) {
		BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_IMAGE_CALCULATED);
		ImBuf *ibuf = sl->equirectangular_irradiance_buffer;
		if (ibuf) {
			/* go over every pixel, determine light, if higher calc direction off the light */
			float new_light;
			float *color = ibuf->rect_float;
			for (int y = 0; y < STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_HEIGHT; y++) {
				for (int x = 0; x < STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_WIDTH; x++) {
					new_light = color[0] + color[1] + color[2];
					if (new_light > best_light) {
						float u = x / (float)STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_WIDTH;
						float v = y / (float)STUDIOLIGHT_IRRADIANCE_EQUIRECTANGULAR_HEIGHT;
						equirectangular_to_direction(sl->light_direction, u, v);
						SWAP(float, sl->light_direction[0], sl->light_direction[1]);
						normalize_v3(sl->light_direction);
						negate_v3(sl->light_direction);
						best_light = new_light;
					}
					color += 4;
				}
			}
		}
	}
	sl->flag |= STUDIOLIGHT_LIGHT_DIRECTION_CALCULATED;
}

static StudioLight *studiolight_add_file(const char *path, int flag)
{
	char filename[FILE_MAXFILE];
	BLI_split_file_part(path, filename, FILE_MAXFILE);
	if (BLI_path_extension_check_array(filename, imb_ext_image)) {
		StudioLight *sl = studiolight_create(STUDIOLIGHT_EXTERNAL_FILE | flag);
		BLI_strncpy(sl->name, filename, FILE_MAXFILE);
		BLI_strncpy(sl->path, path, FILE_MAXFILE);
		sl->path_irr_cache = BLI_string_joinN(path, ".irr");
		sl->path_sh_cache = BLI_string_joinN(path, ".sh2");
		BLI_addtail(&studiolights, sl);
		return sl;
	}
	return NULL;
}

static void studiolight_add_files_from_datafolder(const int folder_id, const char *subfolder, int flag)
{
	struct direntry *dir;
	const char *folder = BKE_appdir_folder_id(folder_id, subfolder);
	if (folder) {
		uint totfile = BLI_filelist_dir_contents(folder, &dir);
		int i;
		for (i = 0; i < totfile; i++) {
			if ((dir[i].type & S_IFREG)) {
				studiolight_add_file(dir[i].path, flag);
			}
		}
		BLI_filelist_free(dir, totfile);
		dir = NULL;
	}
}

static int studiolight_flag_cmp_order(const StudioLight *sl)
{
	/* Internal studiolights before external studio lights */
	if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
		return 1;
	}
	return 0;
}

static int studiolight_cmp(const void *a, const void *b)
{
	const StudioLight *sl1 = a;
	const StudioLight *sl2 = b;

	const int flagorder1 = studiolight_flag_cmp_order(sl1);
	const int flagorder2 = studiolight_flag_cmp_order(sl2);

	if (flagorder1 < flagorder2) {
		return -1;
	}
	else if (flagorder1 > flagorder2) {
		return 1;
	}
	else {
		return BLI_strcasecmp(sl1->name, sl2->name);
	}
}

/* icons */

/* Takes normalized uvs as parameter (range from 0 to 1).
 * inner_edge and outer_edge are distances (from the center)
 * in uv space for the alpha mask falloff. */
static uint alpha_circle_mask(float u, float v, float inner_edge, float outer_edge)
{
	/* Coords from center. */
	float co[2] = {u - 0.5f, v - 0.5f};
	float dist = len_v2(co);
	float alpha = 1.0f + (inner_edge - dist) / (outer_edge - inner_edge);
	uint mask = (uint)floorf(255.0f * min_ff(max_ff(alpha, 0.0f), 1.0f));
	return mask << 24;
}

#define STUDIOLIGHT_DIAMETER 0.95f

static void studiolight_radiance_preview(uint *icon_buffer, StudioLight *sl)
{
	BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);

	float pixel_size = 1.0f / (float)STUDIOLIGHT_ICON_SIZE;

	int offset = 0;
	for (int y = 0; y < STUDIOLIGHT_ICON_SIZE; y++) {
		float dy = (y + 0.5f) / (float)STUDIOLIGHT_ICON_SIZE;
		dy = dy / STUDIOLIGHT_DIAMETER - (1.0f - STUDIOLIGHT_DIAMETER) / 2.0f;
		for (int x = 0; x < STUDIOLIGHT_ICON_SIZE; x++) {
			float dx = (x + 0.5f) / (float)STUDIOLIGHT_ICON_SIZE;
			dx = dx / STUDIOLIGHT_DIAMETER - (1.0f - STUDIOLIGHT_DIAMETER) / 2.0f;

			uint pixelresult = 0x0;
			uint alphamask = alpha_circle_mask(dx, dy, 0.5f - pixel_size, 0.5f);
			if (alphamask != 0) {
				float incoming[3] = {0.0f, 0.0f, -1.0f};

				float normal[3];
				normal[0] = dx * 2.0f - 1.0f;
				normal[1] = dy * 2.0f - 1.0f;
				float dist = len_v2(normal);
				normal[2] = sqrtf(1.0f - SQUARE(dist));

				float direction[3];
				reflect_v3_v3v3(direction, incoming, normal);

				/* We want to see horizon not poles. */
				SWAP(float, direction[1], direction[2]);
				direction[1] = -direction[1];

				float color[4];
				studiolight_calculate_radiance(sl->equirectangular_radiance_buffer, color, direction);

				pixelresult = rgb_to_cpack(
				        linearrgb_to_srgb(color[0]),
				        linearrgb_to_srgb(color[1]),
				        linearrgb_to_srgb(color[2])) | alphamask;
			}
			icon_buffer[offset++] = pixelresult;
		}
	}
}

static void studiolight_matcap_preview(uint *icon_buffer, StudioLight *sl, bool flipped)
{
	BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);

	float color[4];
	float fx, fy;
	float pixel_size = 1.0f / (float)STUDIOLIGHT_ICON_SIZE;
	int offset = 0;
	ImBuf *ibuf = sl->equirectangular_radiance_buffer;

	for (int y = 0; y < STUDIOLIGHT_ICON_SIZE; y++) {
		fy = (y + 0.5f) / (float)STUDIOLIGHT_ICON_SIZE;
		fy = fy / STUDIOLIGHT_DIAMETER - (1.0f - STUDIOLIGHT_DIAMETER) / 2.0f;
		for (int x = 0; x < STUDIOLIGHT_ICON_SIZE; x++) {
			fx = (x + 0.5f) / (float)STUDIOLIGHT_ICON_SIZE;
			fx = fx / STUDIOLIGHT_DIAMETER - (1.0f - STUDIOLIGHT_DIAMETER) / 2.0f;
			if (flipped) {
				fx = 1.0f - fx;
			}
			nearest_interpolation_color(ibuf, NULL, color, fx * ibuf->x - 1.0f, fy * ibuf->y - 1.0f);

			uint alphamask = alpha_circle_mask(fx, fy, 0.5f - pixel_size, 0.5f);

			icon_buffer[offset++] = rgb_to_cpack(
			        linearrgb_to_srgb(color[0]),
			        linearrgb_to_srgb(color[1]),
			        linearrgb_to_srgb(color[2])) | alphamask;
		}
	}
}

static void studiolight_irradiance_preview(uint *icon_buffer, StudioLight *sl)
{
	BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED);

	float pixel_size = 1.0f / (float)STUDIOLIGHT_ICON_SIZE;

	int offset = 0;
	for (int y = 0; y < STUDIOLIGHT_ICON_SIZE; y++) {
		float dy = (y + 0.5f) / (float)STUDIOLIGHT_ICON_SIZE;
		dy = dy / STUDIOLIGHT_DIAMETER - (1.0f - STUDIOLIGHT_DIAMETER) / 2.0f;
		for (int x = 0; x < STUDIOLIGHT_ICON_SIZE; x++) {
			float dx = (x + 0.5f) / (float)STUDIOLIGHT_ICON_SIZE;
			dx = dx / STUDIOLIGHT_DIAMETER - (1.0f - STUDIOLIGHT_DIAMETER) / 2.0f;

			uint pixelresult = 0x0;
			uint alphamask = alpha_circle_mask(dx, dy, 0.5f - pixel_size, 0.5f);
			if (alphamask != 0) {
				/* calculate normal */
				float normal[3];
				normal[0] = dx * 2.0f - 1.0f;
				normal[1] = -(dy * 2.0f - 1.0f);
				float dist = len_v2(normal);
				normal[2] = -sqrtf(1.0f - SQUARE(dist));
				SWAP(float, normal[1], normal[2]);

				float color[3];
				studiolight_sample_spherical_harmonics(sl, color, normal);
				pixelresult = rgb_to_cpack(
				        linearrgb_to_srgb(color[0]),
				        linearrgb_to_srgb(color[1]),
				        linearrgb_to_srgb(color[2])) | alphamask;
			}
			icon_buffer[offset++] = pixelresult;
		}
	}
}

/* API */
void BKE_studiolight_init(void)
{
	StudioLight *sl;
	/* go over the preset folder and add a studiolight for every image with its path */
	/* order studio lights by name */
	/* Also reserve icon space for it. */
	/* Add default studio light */
	sl = studiolight_create(STUDIOLIGHT_INTERNAL | STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED | STUDIOLIGHT_ORIENTATION_CAMERA);
	BLI_strncpy(sl->name, "Default", FILE_MAXFILE);


	copy_v3_fl3(sl->spherical_harmonics_coefs[0], 1.03271556f, 1.07163882f, 1.11193657f);
#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL > 0
	copy_v3_fl3(sl->spherical_harmonics_coefs[1], -0.00480952f, 0.05290511f, 0.16394117f);
	copy_v3_fl3(sl->spherical_harmonics_coefs[2], -0.29686999f, -0.27378261f, -0.24797194f);
	copy_v3_fl3(sl->spherical_harmonics_coefs[3], 0.47932500f, 0.48242140f, 0.47190312f);
#endif
#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL > 1
	copy_v3_fl3(sl->spherical_harmonics_coefs[4], -0.00576984f, 0.00504886f, 0.01640534f);
	copy_v3_fl3(sl->spherical_harmonics_coefs[5], 0.15500379f, 0.15415503f, 0.16244425f);
	copy_v3_fl3(sl->spherical_harmonics_coefs[6], -0.02483751f, -0.02245096f, -0.00536885f);
	copy_v3_fl3(sl->spherical_harmonics_coefs[7], 0.11155496f, 0.11005443f, 0.10839636f);
	copy_v3_fl3(sl->spherical_harmonics_coefs[8], 0.01363425f, 0.01278363f, -0.00159006f);
#endif

	BLI_addtail(&studiolights, sl);

	studiolight_add_files_from_datafolder(BLENDER_SYSTEM_DATAFILES, STUDIOLIGHT_CAMERA_FOLDER, STUDIOLIGHT_ORIENTATION_CAMERA);
	studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,   STUDIOLIGHT_CAMERA_FOLDER, STUDIOLIGHT_ORIENTATION_CAMERA | STUDIOLIGHT_USER_DEFINED);
	studiolight_add_files_from_datafolder(BLENDER_SYSTEM_DATAFILES, STUDIOLIGHT_WORLD_FOLDER,  STUDIOLIGHT_ORIENTATION_WORLD);
	studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,   STUDIOLIGHT_WORLD_FOLDER,  STUDIOLIGHT_ORIENTATION_WORLD | STUDIOLIGHT_USER_DEFINED);
	studiolight_add_files_from_datafolder(BLENDER_SYSTEM_DATAFILES, STUDIOLIGHT_MATCAP_FOLDER, STUDIOLIGHT_ORIENTATION_VIEWNORMAL);
	studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,   STUDIOLIGHT_MATCAP_FOLDER, STUDIOLIGHT_ORIENTATION_VIEWNORMAL | STUDIOLIGHT_USER_DEFINED);

	/* sort studio lights on filename. */
	BLI_listbase_sort(&studiolights, studiolight_cmp);
}

void BKE_studiolight_free(void)
{
	struct StudioLight *sl;
	while ((sl = BLI_pophead(&studiolights))) {
		studiolight_free(sl);
	}
}

struct StudioLight *BKE_studiolight_find_first(int flag)
{
	LISTBASE_FOREACH(StudioLight *, sl, &studiolights) {
		if ((sl->flag & flag)) {
			return sl;
		}
	}
	return NULL;
}

struct StudioLight *BKE_studiolight_find(const char *name, int flag)
{
	LISTBASE_FOREACH(StudioLight *, sl, &studiolights) {
		if (STREQLEN(sl->name, name, FILE_MAXFILE)) {
			if ((sl->flag & flag)) {
				return sl;
			}
			else {
				/* flags do not match, so use default */
				return BKE_studiolight_find_first(flag);
			}
		}
	}
	/* When not found, use the default studio light */
	return BKE_studiolight_find_first(flag);
}

struct StudioLight *BKE_studiolight_findindex(int index, int flag)
{
	LISTBASE_FOREACH(StudioLight *, sl, &studiolights) {
		if (sl->index == index) {
			return sl;
		}
	}
	/* When not found, use the default studio light */
	return BKE_studiolight_find_first(flag);
}

struct ListBase *BKE_studiolight_listbase(void)
{
	return &studiolights;
}

void BKE_studiolight_preview(uint *icon_buffer, StudioLight *sl, int icon_id_type)
{
	switch (icon_id_type) {
		case STUDIOLIGHT_ICON_ID_TYPE_RADIANCE:
		default:
		{
			studiolight_radiance_preview(icon_buffer, sl);
			break;
		}
		case STUDIOLIGHT_ICON_ID_TYPE_IRRADIANCE:
		{
			studiolight_irradiance_preview(icon_buffer, sl);
			break;
		}
		case STUDIOLIGHT_ICON_ID_TYPE_MATCAP:
		{
			studiolight_matcap_preview(icon_buffer, sl, false);
			break;
		}
		case STUDIOLIGHT_ICON_ID_TYPE_MATCAP_FLIPPED:
		{
			studiolight_matcap_preview(icon_buffer, sl, true);
			break;
		}
	}
}

/* Ensure state of Studiolights */
void BKE_studiolight_ensure_flag(StudioLight *sl, int flag)
{
	if ((sl->flag & flag) == flag) {
		return;
	}

	if ((flag & STUDIOLIGHT_EXTERNAL_IMAGE_LOADED)) {
		studiolight_load_equirectangular_image(sl);
	}
	if ((flag & STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED)) {
		studiolight_calculate_radiance_cubemap_buffers(sl);
	}
	if ((flag & STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED)) {
		if (!studiolight_load_spherical_harmonics_coefficients(sl)) {
			studiolight_calculate_diffuse_light(sl);
		}
	}
	if ((flag & STUDIOLIGHT_EQUIRECTANGULAR_RADIANCE_GPUTEXTURE)) {
		studiolight_create_equirectangular_radiance_gputexture(sl);
	}
	if ((flag & STUDIOLIGHT_LIGHT_DIRECTION_CALCULATED)) {
		studiolight_calculate_light_direction(sl);
	}
	if ((flag & STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_GPUTEXTURE)) {
		studiolight_create_equirectangular_irradiance_gputexture(sl);
	}
	if ((flag & STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_IMAGE_CALCULATED)) {
		if (!studiolight_load_irradiance_equirectangular_image(sl)) {
			studiolight_calculate_irradiance_equirectangular_image(sl);
		}
	}
}

/*
 * Python API Functions
 */
void BKE_studiolight_remove(StudioLight *sl)
{
	if (sl->flag & STUDIOLIGHT_USER_DEFINED) {
		BLI_remlink(&studiolights, sl);
		studiolight_free(sl);
	}
}

StudioLight *BKE_studiolight_new(const char *path, int orientation)
{
	StudioLight *sl = studiolight_add_file(path, orientation | STUDIOLIGHT_USER_DEFINED);
	return sl;
}

void BKE_studiolight_refresh(void)
{
	BKE_studiolight_free();
	BKE_studiolight_init();
}

void BKE_studiolight_set_free_function(StudioLight *sl, StudioLightFreeFunction *free_function, void *data)
{
	sl->free_function = free_function;
	sl->free_function_data = data;
}

void BKE_studiolight_unset_icon_id(StudioLight *sl, int icon_id)
{
	BLI_assert(sl != NULL);
	if (sl->icon_id_radiance == icon_id) {
		sl->icon_id_radiance = 0;
	}
	if (sl->icon_id_irradiance == icon_id) {
		sl->icon_id_irradiance = 0;
	}
	if (sl->icon_id_matcap == icon_id) {
		sl->icon_id_matcap = 0;
	}
	if (sl->icon_id_matcap_flipped == icon_id) {
		sl->icon_id_matcap_flipped = 0;
	}
}
