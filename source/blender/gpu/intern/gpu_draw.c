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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_draw.c
 *  \ingroup gpu
 *
 * Utility functions for dealing with OpenGL texture & material context,
 * mipmap generation and light objects.
 *
 * These are some obscure rendering functions shared between the game engine (not anymore)
 * and the blender, in this module to avoid duplication
 * and abstract them away from the rest a bit.
 */

#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_view3d_types.h"
#include "DNA_particle_types.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_bmfont.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_DerivedMesh.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_material.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "PIL_time.h"

#ifdef WITH_SMOKE
#  include "smoke_API.h"
#endif

#ifdef WITH_OPENSUBDIV
#  include "BKE_subsurf.h"
#  include "BKE_editmesh.h"

#  include "gpu_codegen.h"
#endif

extern Material defmaterial; /* from material.c */

//* Checking powers of two for images since OpenGL ES requires it */
#ifdef WITH_DDS
static bool is_power_of_2_resolution(int w, int h)
{
	return is_power_of_2_i(w) && is_power_of_2_i(h);
}
#endif

static bool is_over_resolution_limit(GLenum textarget, int w, int h)
{
	int size = (textarget == GL_TEXTURE_2D) ?
	        GPU_max_texture_size() : GPU_max_cube_map_size();
	int reslimit = (U.glreslimit != 0) ?
	        min_ii(U.glreslimit, size) : size;

	return (w > reslimit || h > reslimit);
}

static int smaller_power_of_2_limit(int num)
{
	int reslimit = (U.glreslimit != 0) ?
	        min_ii(U.glreslimit, GPU_max_texture_size()) :
	        GPU_max_texture_size();
	/* take texture clamping into account */
	if (num > reslimit)
		return reslimit;

	return power_of_2_min_i(num);
}

/* Current OpenGL state caching for GPU_set_tpage */

static struct GPUTextureState {
	/* also controls min/mag filtering */
	bool domipmap;
	/* only use when 'domipmap' is set */
	bool linearmipmap;
	/* store this so that new images created while texture painting won't be set to mipmapped */
	bool texpaint;

	float anisotropic;
	int gpu_mipmap;
} GTS = {1, 0, 0, 1.0f, 0};

/* Mipmap settings */

void GPU_set_gpu_mipmapping(Main *bmain, int gpu_mipmap)
{
	int old_value = GTS.gpu_mipmap;

	/* only actually enable if it's supported */
	GTS.gpu_mipmap = gpu_mipmap;

	if (old_value != GTS.gpu_mipmap) {
		GPU_free_images(bmain);
	}
}

void GPU_set_mipmap(Main *bmain, bool mipmap)
{
	if (GTS.domipmap != mipmap) {
		GPU_free_images(bmain);
		GTS.domipmap = mipmap;
	}
}

void GPU_set_linear_mipmap(bool linear)
{
	if (GTS.linearmipmap != linear) {
		GTS.linearmipmap = linear;
	}
}

bool GPU_get_mipmap(void)
{
	return GTS.domipmap && !GTS.texpaint;
}

bool GPU_get_linear_mipmap(void)
{
	return GTS.linearmipmap;
}

static GLenum gpu_get_mipmap_filter(bool mag)
{
	/* linearmipmap is off by default *when mipmapping is off,
	 * use unfiltered display */
	if (mag) {
		if (GTS.domipmap)
			return GL_LINEAR;
		else
			return GL_NEAREST;
	}
	else {
		if (GTS.domipmap) {
			if (GTS.linearmipmap) {
				return GL_LINEAR_MIPMAP_LINEAR;
			}
			else {
				return GL_LINEAR_MIPMAP_NEAREST;
			}
		}
		else {
			return GL_NEAREST;
		}
	}
}

/* Anisotropic filtering settings */
void GPU_set_anisotropic(Main *bmain, float value)
{
	if (GTS.anisotropic != value) {
		GPU_free_images(bmain);

		/* Clamp value to the maximum value the graphics card supports */
		const float max = GPU_max_texture_anisotropy();
		if (value > max)
			value = max;

		GTS.anisotropic = value;
	}
}

float GPU_get_anisotropic(void)
{
	return GTS.anisotropic;
}

/* Set OpenGL state for an MTFace */

static GPUTexture **gpu_get_image_gputexture(Image *ima, GLenum textarget)
{
	if (textarget == GL_TEXTURE_2D)
		return &ima->gputexture[TEXTARGET_TEXTURE_2D];
	else if (textarget == GL_TEXTURE_CUBE_MAP)
		return &ima->gputexture[TEXTARGET_TEXTURE_CUBE_MAP];

	return NULL;
}

typedef struct VerifyThreadData {
	ImBuf *ibuf;
	float *srgb_frect;
} VerifyThreadData;

static void gpu_verify_high_bit_srgb_buffer_slice(
        float *srgb_frect,
        ImBuf *ibuf,
        const int start_line,
        const int height)
{
	size_t offset = ibuf->channels * start_line * ibuf->x;
	float *current_srgb_frect = srgb_frect + offset;
	float *current_rect_float = ibuf->rect_float + offset;
	IMB_buffer_float_from_float(
	        current_srgb_frect,
	        current_rect_float,
	        ibuf->channels,
	        IB_PROFILE_SRGB,
	        IB_PROFILE_LINEAR_RGB, true,
	        ibuf->x, height,
	        ibuf->x, ibuf->x);
	IMB_buffer_float_unpremultiply(current_srgb_frect, ibuf->x, height);
}

static void verify_thread_do(
        void *data_v,
        int start_scanline,
        int num_scanlines)
{
	VerifyThreadData *data = (VerifyThreadData *)data_v;
	gpu_verify_high_bit_srgb_buffer_slice(
	        data->srgb_frect,
	        data->ibuf,
	        start_scanline,
	        num_scanlines);
}

static void gpu_verify_high_bit_srgb_buffer(
        float *srgb_frect,
        ImBuf *ibuf)
{
	if (ibuf->y < 64) {
		gpu_verify_high_bit_srgb_buffer_slice(
		        srgb_frect,
		        ibuf,
		        0, ibuf->y);
	}
	else {
		VerifyThreadData data;
		data.ibuf = ibuf;
		data.srgb_frect = srgb_frect;
		IMB_processor_apply_threaded_scanlines(ibuf->y, verify_thread_do, &data);
	}
}

GPUTexture *GPU_texture_from_blender(
        Image *ima,
        ImageUser *iuser,
        int textarget,
        bool is_data,
        double UNUSED(time))
{
	if (ima == NULL) {
		return NULL;
	}

	/* Test if we already have a texture. */
	GPUTexture **tex = gpu_get_image_gputexture(ima, textarget);
	if (*tex) {
		return *tex;
	}

	/* Check if we have a valid image. If not, we return a dummy
	 * texture with zero bindcode so we don't keep trying. */
	unsigned int bindcode = 0;
	if (ima->ok == 0) {
		*tex = GPU_texture_from_bindcode(textarget, bindcode);
		return *tex;
	}

	/* currently, tpage refresh is used by ima sequences */
	if (ima->tpageflag & IMA_TPAGE_REFRESH) {
		GPU_free_image(ima);
		ima->tpageflag &= ~IMA_TPAGE_REFRESH;
	}

	/* check if we have a valid image buffer */
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
	if (ibuf == NULL) {
		*tex = GPU_texture_from_bindcode(textarget, bindcode);
		return *tex;
	}

	/* flag to determine whether deep format is used */
	bool use_high_bit_depth = false, do_color_management = false;

	if (ibuf->rect_float) {
		if (U.use_16bit_textures) {
			/* use high precision textures. This is relatively harmless because OpenGL gives us
			 * a high precision format only if it is available */
			use_high_bit_depth = true;
		}
		else if (ibuf->rect == NULL) {
			IMB_rect_from_float(ibuf);
		}
		/* we may skip this in high precision, but if not, we need to have a valid buffer here */
		else if (ibuf->userflags & IB_RECT_INVALID) {
			IMB_rect_from_float(ibuf);
		}

		/* TODO unneeded when float images are correctly treated as linear always */
		if (!is_data) {
			do_color_management = true;
		}
	}

	const int rectw = ibuf->x;
	const int recth = ibuf->y;
	unsigned int *rect = ibuf->rect;
	float *frect = NULL;
	float *srgb_frect = NULL;

	if (use_high_bit_depth) {
		if (do_color_management) {
			frect = srgb_frect = MEM_mallocN(ibuf->x * ibuf->y * sizeof(*srgb_frect) * 4, "floar_buf_col_cor");
			gpu_verify_high_bit_srgb_buffer(srgb_frect, ibuf);
		}
		else {
			frect = ibuf->rect_float;
		}
	}

	const bool mipmap = GPU_get_mipmap();

#ifdef WITH_DDS
	if (ibuf->ftype == IMB_FTYPE_DDS) {
		GPU_create_gl_tex_compressed(&bindcode, rect, rectw, recth, textarget, mipmap, ima, ibuf);
	}
	else
#endif
	{
		GPU_create_gl_tex(&bindcode, rect, frect, rectw, recth, textarget, mipmap, use_high_bit_depth, ima);
	}

	/* mark as non-color data texture */
	if (bindcode) {
		if (is_data)
			ima->tpageflag |= IMA_GLBIND_IS_DATA;
		else
			ima->tpageflag &= ~IMA_GLBIND_IS_DATA;
	}

	/* clean up */
	if (srgb_frect)
		MEM_freeN(srgb_frect);

	BKE_image_release_ibuf(ima, ibuf, NULL);

	*tex = GPU_texture_from_bindcode(textarget, bindcode);
	return *tex;
}

static void **gpu_gen_cube_map(unsigned int *rect, float *frect, int rectw, int recth, bool use_high_bit_depth)
{
	size_t block_size = use_high_bit_depth ? sizeof(float) * 4 : sizeof(unsigned char) * 4;
	void **sides = NULL;
	int h = recth / 2;
	int w = rectw / 3;

	if ((use_high_bit_depth && frect == NULL) || (!use_high_bit_depth && rect == NULL) || w != h)
		return sides;

	/* PosX, NegX, PosY, NegY, PosZ, NegZ */
	sides = MEM_mallocN(sizeof(void *) * 6, "");
	for (int i = 0; i < 6; i++)
		sides[i] = MEM_mallocN(block_size * w * h, "");

	/* divide image into six parts */
	/* ______________________
	 * |      |      |      |
	 * | NegX | NegY | PosX |
	 * |______|______|______|
	 * |      |      |      |
	 * | NegZ | PosZ | PosY |
	 * |______|______|______|
	 */
	if (use_high_bit_depth) {
		float (*frectb)[4] = (float(*)[4])frect;
		float (**fsides)[4] = (float(**)[4])sides;

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				memcpy(&fsides[0][x * h + y], &frectb[(recth - y - 1) * rectw + 2 * w + x], block_size);
				memcpy(&fsides[1][x * h + y], &frectb[(y + h) * rectw + w - 1 - x], block_size);
				memcpy(&fsides[3][y * w + x], &frectb[(recth - y - 1) * rectw + 2 * w - 1 - x], block_size);
				memcpy(&fsides[5][y * w + x], &frectb[(h - y - 1) * rectw + w - 1 - x], block_size);
			}
			memcpy(&fsides[2][y * w], frectb[y * rectw + 2 * w], block_size * w);
			memcpy(&fsides[4][y * w], frectb[y * rectw + w], block_size * w);
		}
	}
	else {
		unsigned int **isides = (unsigned int **)sides;

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				isides[0][x * h + y] = rect[(recth - y - 1) * rectw + 2 * w + x];
				isides[1][x * h + y] = rect[(y + h) * rectw + w - 1 - x];
				isides[3][y * w + x] = rect[(recth - y - 1) * rectw + 2 * w - 1 - x];
				isides[5][y * w + x] = rect[(h - y - 1) * rectw + w - 1 - x];
			}
			memcpy(&isides[2][y * w], &rect[y * rectw + 2 * w], block_size * w);
			memcpy(&isides[4][y * w], &rect[y * rectw + w], block_size * w);
		}
	}

	return sides;
}

static void gpu_del_cube_map(void **cube_map)
{
	int i;
	if (cube_map == NULL)
		return;
	for (i = 0; i < 6; i++)
		MEM_freeN(cube_map[i]);
	MEM_freeN(cube_map);
}

/* Image *ima can be NULL */
void GPU_create_gl_tex(
        unsigned int *bind, unsigned int *rect, float *frect, int rectw, int recth,
        int textarget, bool mipmap, bool use_high_bit_depth, Image *ima)
{
	ImBuf *ibuf = NULL;

	int tpx = rectw;
	int tpy = recth;

	/* create image */
	glGenTextures(1, (GLuint *)bind);
	glBindTexture(textarget, *bind);

	if (textarget == GL_TEXTURE_2D) {
		if (use_high_bit_depth) {
			if (GLEW_ARB_texture_float)
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, rectw, recth, 0, GL_RGBA, GL_FLOAT, frect);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, rectw, recth, 0, GL_RGBA, GL_FLOAT, frect);
		}
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rectw, recth, 0, GL_RGBA, GL_UNSIGNED_BYTE, rect);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));

		if (GPU_get_mipmap() && mipmap) {
			if (GTS.gpu_mipmap) {
				glGenerateMipmap(GL_TEXTURE_2D);
			}
			else {
				int i;
				if (!ibuf) {
					if (use_high_bit_depth) {
						ibuf = IMB_allocFromBuffer(NULL, frect, tpx, tpy);
					}
					else {
						ibuf = IMB_allocFromBuffer(rect, NULL, tpx, tpy);
					}
				}
				IMB_makemipmap(ibuf, true);

				for (i = 1; i < ibuf->miptot; i++) {
					ImBuf *mip = ibuf->mipmap[i - 1];
					if (use_high_bit_depth) {
						if (GLEW_ARB_texture_float)
							glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA16F, mip->x, mip->y, 0, GL_RGBA, GL_FLOAT, mip->rect_float);
						else
							glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA16, mip->x, mip->y, 0, GL_RGBA, GL_FLOAT, mip->rect_float);
					}
					else {
						glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA8, mip->x, mip->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, mip->rect);
					}
				}
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gpu_get_mipmap_filter(0));
			if (ima)
				ima->tpageflag |= IMA_MIPMAP_COMPLETE;
		}
		else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
	}
	else if (textarget == GL_TEXTURE_CUBE_MAP) {
		int w = rectw / 3, h = recth / 2;

		if (h == w && is_power_of_2_i(h) && !is_over_resolution_limit(textarget, h, w)) {
			void **cube_map = gpu_gen_cube_map(rect, frect, rectw, recth, use_high_bit_depth);
			GLenum informat = use_high_bit_depth ? (GLEW_ARB_texture_float ? GL_RGBA16F : GL_RGBA16) : GL_RGBA8;
			GLenum type = use_high_bit_depth ? GL_FLOAT : GL_UNSIGNED_BYTE;

			if (cube_map)
				for (int i = 0; i < 6; i++)
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, informat, w, h, 0, GL_RGBA, type, cube_map[i]);

			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));

			if (GPU_get_mipmap() && mipmap) {
				if (GTS.gpu_mipmap) {
					glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
				}
				else {
					if (!ibuf) {
						if (use_high_bit_depth) {
							ibuf = IMB_allocFromBuffer(NULL, frect, tpx, tpy);
						}
						else {
							ibuf = IMB_allocFromBuffer(rect, NULL, tpx, tpy);
						}
					}

					IMB_makemipmap(ibuf, true);

					for (int i = 1; i < ibuf->miptot; i++) {
						ImBuf *mip = ibuf->mipmap[i - 1];
						void **mip_cube_map = gpu_gen_cube_map(
						        mip->rect, mip->rect_float,
						        mip->x, mip->y, use_high_bit_depth);
						int mipw = mip->x / 3, miph = mip->y / 2;

						if (mip_cube_map) {
							for (int j = 0; j < 6; j++) {
								glTexImage2D(
								        GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, i,
								        informat, mipw, miph, 0, GL_RGBA, type, mip_cube_map[j]);
							}
						}
						gpu_del_cube_map(mip_cube_map);
					}
				}
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, gpu_get_mipmap_filter(0));

				if (ima)
					ima->tpageflag |= IMA_MIPMAP_COMPLETE;
			}
			else {
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			}
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

			gpu_del_cube_map(cube_map);
		}
		else {
			printf("Incorrect envmap size\n");
		}
	}

	if (GLEW_EXT_texture_filter_anisotropic)
		glTexParameterf(textarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, GPU_get_anisotropic());

	glBindTexture(textarget, 0);

	if (ibuf)
		IMB_freeImBuf(ibuf);
}

/**
 * GPU_upload_dxt_texture() assumes that the texture is already bound and ready to go.
 * This is so the viewport and the BGE can share some code.
 * Returns false if the provided ImBuf doesn't have a supported DXT compression format
 */
bool GPU_upload_dxt_texture(ImBuf *ibuf)
{
#ifdef WITH_DDS
	GLint format = 0;
	int blocksize, height, width, i, size, offset = 0;

	width = ibuf->x;
	height = ibuf->y;

	if (GLEW_EXT_texture_compression_s3tc) {
		if (ibuf->dds_data.fourcc == FOURCC_DXT1)
			format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		else if (ibuf->dds_data.fourcc == FOURCC_DXT3)
			format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		else if (ibuf->dds_data.fourcc == FOURCC_DXT5)
			format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	}

	if (format == 0) {
		fprintf(stderr, "Unable to find a suitable DXT compression, falling back to uncompressed\n");
		return false;
	}

	if (!is_power_of_2_resolution(width, height)) {
		fprintf(stderr, "Unable to load non-power-of-two DXT image resolution, falling back to uncompressed\n");
		return false;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gpu_get_mipmap_filter(0));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));

	if (GLEW_EXT_texture_filter_anisotropic)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GPU_get_anisotropic());

	blocksize = (ibuf->dds_data.fourcc == FOURCC_DXT1) ? 8 : 16;
	for (i = 0; i < ibuf->dds_data.nummipmaps && (width || height); ++i) {
		if (width == 0)
			width = 1;
		if (height == 0)
			height = 1;

		size = ((width + 3) / 4) * ((height + 3) / 4) * blocksize;

		glCompressedTexImage2D(
		        GL_TEXTURE_2D, i, format, width, height,
		        0, size, ibuf->dds_data.data + offset);

		offset += size;
		width >>= 1;
		height >>= 1;
	}

	/* set number of mipmap levels we have, needed in case they don't go down to 1x1 */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, i - 1);

	return true;
#else
	(void)ibuf;
	return false;
#endif
}

void GPU_create_gl_tex_compressed(
        unsigned int *bind, unsigned int *pix, int x, int y,
        int textarget, int mipmap, Image *ima, ImBuf *ibuf)
{
#ifndef WITH_DDS
	(void)ibuf;
	/* Fall back to uncompressed if DDS isn't enabled */
	GPU_create_gl_tex(bind, pix, NULL, x, y, textarget, mipmap, 0, ima);
#else
	glGenTextures(1, (GLuint *)bind);
	glBindTexture(textarget, *bind);

	if (textarget == GL_TEXTURE_2D && GPU_upload_dxt_texture(ibuf) == 0) {
		glDeleteTextures(1, (GLuint *)bind);
		GPU_create_gl_tex(bind, pix, NULL, x, y, textarget, mipmap, 0, ima);
	}

	glBindTexture(textarget, 0);
#endif
}

/* these two functions are called on entering and exiting texture paint mode,
 * temporary disabling/enabling mipmapping on all images for quick texture
 * updates with glTexSubImage2D. images that didn't change don't have to be
 * re-uploaded to OpenGL */
void GPU_paint_set_mipmap(Main *bmain, bool mipmap)
{
	if (!GTS.domipmap)
		return;

	GTS.texpaint = !mipmap;

	if (mipmap) {
		for (Image *ima = bmain->image.first; ima; ima = ima->id.next) {
			if (BKE_image_has_opengl_texture(ima)) {
				if (ima->tpageflag & IMA_MIPMAP_COMPLETE) {
					if (ima->gputexture[TEXTARGET_TEXTURE_2D]) {
						GPU_texture_bind(ima->gputexture[TEXTARGET_TEXTURE_2D], 0);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gpu_get_mipmap_filter(0));
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));
						GPU_texture_unbind(ima->gputexture[TEXTARGET_TEXTURE_2D]);
					}
				}
				else
					GPU_free_image(ima);
			}
			else
				ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
		}

	}
	else {
		for (Image *ima = bmain->image.first; ima; ima = ima->id.next) {
			if (BKE_image_has_opengl_texture(ima)) {
				if (ima->gputexture[TEXTARGET_TEXTURE_2D]) {
					GPU_texture_bind(ima->gputexture[TEXTARGET_TEXTURE_2D], 0);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));
					GPU_texture_unbind(ima->gputexture[TEXTARGET_TEXTURE_2D]);
				}
			}
			else
				ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
		}
	}
}


/* check if image has been downscaled and do scaled partial update */
static bool gpu_check_scaled_image(ImBuf *ibuf, Image *ima, float *frect, int x, int y, int w, int h)
{
	if (is_over_resolution_limit(GL_TEXTURE_2D, ibuf->x, ibuf->y)) {
		int x_limit = smaller_power_of_2_limit(ibuf->x);
		int y_limit = smaller_power_of_2_limit(ibuf->y);

		float xratio = x_limit / (float)ibuf->x;
		float yratio = y_limit / (float)ibuf->y;

		/* find new width, height and x,y gpu texture coordinates */

		/* take ceiling because we will be losing 1 pixel due to rounding errors in x,y... */
		int rectw = (int)ceil(xratio * w);
		int recth = (int)ceil(yratio * h);

		x *= xratio;
		y *= yratio;

		/* ...but take back if we are over the limit! */
		if (rectw + x > x_limit) rectw--;
		if (recth + y > y_limit) recth--;

		GPU_texture_bind(ima->gputexture[TEXTARGET_TEXTURE_2D], 0);

		/* float rectangles are already continuous in memory so we can use IMB_scaleImBuf */
		if (frect) {
			ImBuf *ibuf_scale = IMB_allocFromBuffer(NULL, frect, w, h);
			IMB_scaleImBuf(ibuf_scale, rectw, recth);

			glTexSubImage2D(
			        GL_TEXTURE_2D, 0, x, y, rectw, recth, GL_RGBA,
			        GL_FLOAT, ibuf_scale->rect_float);

			IMB_freeImBuf(ibuf_scale);
		}
		/* byte images are not continuous in memory so do manual interpolation */
		else {
			unsigned char *scalerect = MEM_mallocN(rectw * recth * sizeof(*scalerect) * 4, "scalerect");
			unsigned int *p = (unsigned int *)scalerect;
			int i, j;
			float inv_xratio = 1.0f / xratio;
			float inv_yratio = 1.0f / yratio;
			for (i = 0; i < rectw; i++) {
				float u = (x + i) * inv_xratio;
				for (j = 0; j < recth; j++) {
					float v = (y + j) * inv_yratio;
					bilinear_interpolation_color_wrap(ibuf, (unsigned char *)(p + i + j * (rectw)), NULL, u, v);
				}
			}

			glTexSubImage2D(
			        GL_TEXTURE_2D, 0, x, y, rectw, recth, GL_RGBA,
			        GL_UNSIGNED_BYTE, scalerect);

			MEM_freeN(scalerect);
		}

		if (GPU_get_mipmap()) {
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		else {
			ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
		}

		GPU_texture_unbind(ima->gputexture[TEXTARGET_TEXTURE_2D]);

		return true;
	}

	return false;
}

void GPU_paint_update_image(Image *ima, ImageUser *iuser, int x, int y, int w, int h)
{
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);

	if ((!GTS.gpu_mipmap && GPU_get_mipmap()) ||
	    (ima->gputexture[TEXTARGET_TEXTURE_2D] == NULL) ||
	    (ibuf == NULL) ||
	    (w == 0) || (h == 0))
	{
		/* these cases require full reload still */
		GPU_free_image(ima);
	}
	else {
		/* for the special case, we can do a partial update
		 * which is much quicker for painting */
		GLint row_length, skip_pixels, skip_rows;

		/* if color correction is needed, we must update the part that needs updating. */
		if (ibuf->rect_float) {
			float *buffer = MEM_mallocN(w * h * sizeof(float) * 4, "temp_texpaint_float_buf");
			bool is_data = (ima->tpageflag & IMA_GLBIND_IS_DATA) != 0;
			IMB_partial_rect_from_float(ibuf, buffer, x, y, w, h, is_data);

			if (gpu_check_scaled_image(ibuf, ima, buffer, x, y, w, h)) {
				MEM_freeN(buffer);
				BKE_image_release_ibuf(ima, ibuf, NULL);
				return;
			}

			GPU_texture_bind(ima->gputexture[TEXTARGET_TEXTURE_2D], 0);
			glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_FLOAT, buffer);

			MEM_freeN(buffer);

			/* we have already accounted for the case where GTS.gpu_mipmap is false
			 * so we will be using GPU mipmap generation here */
			if (GPU_get_mipmap()) {
				glGenerateMipmap(GL_TEXTURE_2D);
			}
			else {
				ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
			}

			GPU_texture_unbind(ima->gputexture[TEXTARGET_TEXTURE_2D]);

			BKE_image_release_ibuf(ima, ibuf, NULL);
			return;
		}

		if (gpu_check_scaled_image(ibuf, ima, NULL, x, y, w, h)) {
			BKE_image_release_ibuf(ima, ibuf, NULL);
			return;
		}

		GPU_texture_bind(ima->gputexture[TEXTARGET_TEXTURE_2D], 0);

		glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
		glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
		glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, ibuf->x);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

		glTexSubImage2D(
		        GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA,
		        GL_UNSIGNED_BYTE, ibuf->rect);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);

		/* see comment above as to why we are using gpu mipmap generation here */
		if (GPU_get_mipmap()) {
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		else {
			ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
		}

		GPU_texture_unbind(ima->gputexture[TEXTARGET_TEXTURE_2D]);
	}

	BKE_image_release_ibuf(ima, ibuf, NULL);
}

void GPU_free_smoke(SmokeModifierData *smd)
{
	if (smd->type & MOD_SMOKE_TYPE_DOMAIN && smd->domain) {
		if (smd->domain->tex)
			GPU_texture_free(smd->domain->tex);
		smd->domain->tex = NULL;

		if (smd->domain->tex_shadow)
			GPU_texture_free(smd->domain->tex_shadow);
		smd->domain->tex_shadow = NULL;

		if (smd->domain->tex_flame)
			GPU_texture_free(smd->domain->tex_flame);
		smd->domain->tex_flame = NULL;
	}
}

void GPU_create_smoke(SmokeModifierData *smd, int highres)
{
#ifdef WITH_SMOKE
	if (smd->type & MOD_SMOKE_TYPE_DOMAIN) {
		SmokeDomainSettings *sds = smd->domain;
		if (!sds->tex && !highres) {
			/* rgba texture for color + density */
			if (smoke_has_colors(sds->fluid)) {
				float *data = MEM_callocN(sizeof(float) * sds->total_cells * 4, "smokeColorTexture");
				smoke_get_rgba(sds->fluid, data, 0);
				sds->tex = GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], GPU_RGBA8, data, NULL);
				MEM_freeN(data);
			}
			/* density only */
			else {
				sds->tex = GPU_texture_create_3D(
				        sds->res[0], sds->res[1], sds->res[2],
				        GPU_R8, smoke_get_density(sds->fluid), NULL);

				/* Swizzle the RGBA components to read the Red channel so
				 * that the shader stay the same for colored and non color
				 * density textures. */
				GPU_texture_bind(sds->tex, 0);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_R, GL_RED);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_G, GL_RED);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_B, GL_RED);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_A, GL_RED);
				GPU_texture_unbind(sds->tex);
			}
			sds->tex_flame = (
			        smoke_has_fuel(sds->fluid) ?
			        GPU_texture_create_3D(
			                sds->res[0], sds->res[1], sds->res[2],
			                GPU_R8, smoke_get_flame(sds->fluid), NULL) :
			        NULL);
		}
		else if (!sds->tex && highres) {
			/* rgba texture for color + density */
			if (smoke_turbulence_has_colors(sds->wt)) {
				float *data = MEM_callocN(sizeof(float) * smoke_turbulence_get_cells(sds->wt) * 4, "smokeColorTexture");
				smoke_turbulence_get_rgba(sds->wt, data, 0);
				sds->tex = GPU_texture_create_3D(sds->res_wt[0], sds->res_wt[1], sds->res_wt[2], GPU_RGBA8, data, NULL);
				MEM_freeN(data);
			}
			/* density only */
			else {
				sds->tex = GPU_texture_create_3D(
				        sds->res_wt[0], sds->res_wt[1], sds->res_wt[2],
				        GPU_R8, smoke_turbulence_get_density(sds->wt), NULL);

				/* Swizzle the RGBA components to read the Red channel so
				 * that the shader stay the same for colored and non color
				 * density textures. */
				GPU_texture_bind(sds->tex, 0);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_R, GL_RED);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_G, GL_RED);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_B, GL_RED);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_A, GL_RED);
				GPU_texture_unbind(sds->tex);
			}
			sds->tex_flame = (
			        smoke_turbulence_has_fuel(sds->wt) ?
			        GPU_texture_create_3D(
			                sds->res_wt[0], sds->res_wt[1], sds->res_wt[2],
			                GPU_R8, smoke_turbulence_get_flame(sds->wt), NULL) :
			        NULL);
		}

		sds->tex_shadow = GPU_texture_create_3D(
		        sds->res[0], sds->res[1], sds->res[2],
		        GPU_R8, sds->shadow, NULL);
	}
#else // WITH_SMOKE
	(void)highres;
	smd->domain->tex = NULL;
	smd->domain->tex_flame = NULL;
	smd->domain->tex_shadow = NULL;
#endif // WITH_SMOKE
}

void GPU_create_smoke_velocity(SmokeModifierData *smd)
{
#ifdef WITH_SMOKE
	if (smd->type & MOD_SMOKE_TYPE_DOMAIN) {
		SmokeDomainSettings *sds = smd->domain;

		const float *vel_x = smoke_get_velocity_x(sds->fluid);
		const float *vel_y = smoke_get_velocity_y(sds->fluid);
		const float *vel_z = smoke_get_velocity_z(sds->fluid);

		if (ELEM(NULL, vel_x, vel_y, vel_z)) {
			return;
		}

		if (!sds->tex_velocity_x) {
			sds->tex_velocity_x = GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], GPU_R16F, vel_x, NULL);
			sds->tex_velocity_y = GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], GPU_R16F, vel_y, NULL);
			sds->tex_velocity_z = GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], GPU_R16F, vel_z, NULL);
		}
	}
#else // WITH_SMOKE
	smd->domain->tex_velocity_x = NULL;
	smd->domain->tex_velocity_y = NULL;
	smd->domain->tex_velocity_z = NULL;
#endif // WITH_SMOKE
}

/* TODO Unify with the other GPU_free_smoke. */
void GPU_free_smoke_velocity(SmokeModifierData *smd)
{
	if (smd->type & MOD_SMOKE_TYPE_DOMAIN && smd->domain) {
		if (smd->domain->tex_velocity_x)
			GPU_texture_free(smd->domain->tex_velocity_x);

		if (smd->domain->tex_velocity_y)
			GPU_texture_free(smd->domain->tex_velocity_y);

		if (smd->domain->tex_velocity_z)
			GPU_texture_free(smd->domain->tex_velocity_z);

		smd->domain->tex_velocity_x = NULL;
		smd->domain->tex_velocity_y = NULL;
		smd->domain->tex_velocity_z = NULL;
	}
}

static LinkNode *image_free_queue = NULL;

static void gpu_queue_image_for_free(Image *ima)
{
	BLI_thread_lock(LOCK_OPENGL);
	BLI_linklist_prepend(&image_free_queue, ima);
	BLI_thread_unlock(LOCK_OPENGL);
}

void GPU_free_unused_buffers(Main *bmain)
{
	if (!BLI_thread_is_main())
		return;

	BLI_thread_lock(LOCK_OPENGL);

	/* images */
	for (LinkNode *node = image_free_queue; node; node = node->next) {
		Image *ima = node->link;

		/* check in case it was freed in the meantime */
		if (bmain && BLI_findindex(&bmain->image, ima) != -1)
			GPU_free_image(ima);
	}

	BLI_linklist_free(image_free_queue, NULL);
	image_free_queue = NULL;

	BLI_thread_unlock(LOCK_OPENGL);
}

void GPU_free_image(Image *ima)
{
	if (!BLI_thread_is_main()) {
		gpu_queue_image_for_free(ima);
		return;
	}

	for (int i = 0; i < TEXTARGET_COUNT; i++) {
		/* free glsl image binding */
		if (ima->gputexture[i]) {
			GPU_texture_free(ima->gputexture[i]);
			ima->gputexture[i] = NULL;
		}
	}

	ima->tpageflag &= ~(IMA_MIPMAP_COMPLETE | IMA_GLBIND_IS_DATA);
}

void GPU_free_images(Main *bmain)
{
	if (bmain) {
		for (Image *ima = bmain->image.first; ima; ima = ima->id.next) {
			GPU_free_image(ima);
		}
	}
}

/* same as above but only free animated images */
void GPU_free_images_anim(Main *bmain)
{
	if (bmain) {
		for (Image *ima = bmain->image.first; ima; ima = ima->id.next) {
			if (BKE_image_is_animated(ima)) {
				GPU_free_image(ima);
			}
		}
	}
}


void GPU_free_images_old(Main *bmain)
{
	static int lasttime = 0;
	int ctime = (int)PIL_check_seconds_timer();

	/*
	 * Run garbage collector once for every collecting period of time
	 * if textimeout is 0, that's the option to NOT run the collector
	 */
	if (U.textimeout == 0 || ctime % U.texcollectrate || ctime == lasttime)
		return;

	/* of course not! */
	if (G.is_rendering)
		return;

	lasttime = ctime;

	Image *ima = bmain->image.first;
	while (ima) {
		if ((ima->flag & IMA_NOCOLLECT) == 0 && ctime - ima->lastused > U.textimeout) {
			/* If it's in GL memory, deallocate and set time tag to current time
			 * This gives textures a "second chance" to be used before dying. */
			if (BKE_image_has_opengl_texture(ima)) {
				GPU_free_image(ima);
				ima->lastused = ctime;
			}
			/* Otherwise, just kill the buffers */
			else {
				BKE_image_free_buffers(ima);
			}
		}
		ima = ima->id.next;
	}
}

static void gpu_disable_multisample(void)
{
#ifdef __linux__
	/* changing multisample from the default (enabled) causes problems on some
	 * systems (NVIDIA/Linux) when the pixel format doesn't have a multisample buffer */
	bool toggle_ok = true;

	if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_UNIX, GPU_DRIVER_ANY)) {
		int samples = 0;
		glGetIntegerv(GL_SAMPLES, &samples);

		if (samples == 0)
			toggle_ok = false;
	}

	if (toggle_ok) {
		glDisable(GL_MULTISAMPLE);
	}
#else
	glDisable(GL_MULTISAMPLE);
#endif
}

/* Default OpenGL State
 *
 * This is called on startup, for opengl offscreen render.
 * Generally we should always return to this state when
 * temporarily modifying the state for drawing, though that are (undocumented)
 * exceptions that we should try to get rid of. */

void GPU_state_init(void)
{
	GPU_disable_program_point_size();

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	glDepthFunc(GL_LEQUAL);

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_COLOR_LOGIC_OP);
	glDisable(GL_STENCIL_TEST);

	glDepthRange(0.0, 1.0);

	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

	gpu_disable_multisample();
}

void GPU_enable_program_point_size(void)
{
	glEnable(GL_PROGRAM_POINT_SIZE);
}

void GPU_disable_program_point_size(void)
{
	glDisable(GL_PROGRAM_POINT_SIZE);
}

/** \name Framebuffer color depth, for selection codes
 * \{ */

#ifdef __APPLE__

/* apple seems to round colors to below and up on some configs */

static unsigned int index_to_framebuffer(int index)
{
	unsigned int i = index;

	switch (GPU_color_depth()) {
		case 12:
			i = ((i & 0xF00) << 12) + ((i & 0xF0) << 8) + ((i & 0xF) << 4);
			/* sometimes dithering subtracts! */
			i |= 0x070707;
			break;
		case 15:
		case 16:
			i = ((i & 0x7C00) << 9) + ((i & 0x3E0) << 6) + ((i & 0x1F) << 3);
			i |= 0x030303;
			break;
		case 24:
			break;
		default: /* 18 bits... */
			i = ((i & 0x3F000) << 6) + ((i & 0xFC0) << 4) + ((i & 0x3F) << 2);
			i |= 0x010101;
			break;
	}

	return i;
}

#else

/* this is the old method as being in use for ages.... seems to work? colors are rounded to lower values */

static unsigned int index_to_framebuffer(int index)
{
	unsigned int i = index;

	switch (GPU_color_depth()) {
		case 8:
			i = ((i & 48) << 18) + ((i & 12) << 12) + ((i & 3) << 6);
			i |= 0x3F3F3F;
			break;
		case 12:
			i = ((i & 0xF00) << 12) + ((i & 0xF0) << 8) + ((i & 0xF) << 4);
			/* sometimes dithering subtracts! */
			i |= 0x0F0F0F;
			break;
		case 15:
		case 16:
			i = ((i & 0x7C00) << 9) + ((i & 0x3E0) << 6) + ((i & 0x1F) << 3);
			i |= 0x070707;
			break;
		case 24:
			break;
		default:    /* 18 bits... */
			i = ((i & 0x3F000) << 6) + ((i & 0xFC0) << 4) + ((i & 0x3F) << 2);
			i |= 0x030303;
			break;
	}

	return i;
}

#endif


void GPU_select_index_set(int index)
{
	const int col = index_to_framebuffer(index);
	glColor3ub(( (col)        & 0xFF),
	           (((col) >>  8) & 0xFF),
	           (((col) >> 16) & 0xFF));
}

void GPU_select_index_get(int index, int *r_col)
{
	const int col = index_to_framebuffer(index);
	char *c_col = (char *)r_col;
	c_col[0] = (col & 0xFF); /* red */
	c_col[1] = ((col >>  8) & 0xFF); /* green */
	c_col[2] = ((col >> 16) & 0xFF); /* blue */
	c_col[3] = 0xFF; /* alpha */
}


#define INDEX_FROM_BUF_8(col)     ((((col) & 0xC00000) >> 18) + (((col) & 0xC000) >> 12) + (((col) & 0xC0) >> 6))
#define INDEX_FROM_BUF_12(col)    ((((col) & 0xF00000) >> 12) + (((col) & 0xF000) >> 8)  + (((col) & 0xF0) >> 4))
#define INDEX_FROM_BUF_15_16(col) ((((col) & 0xF80000) >> 9)  + (((col) & 0xF800) >> 6)  + (((col) & 0xF8) >> 3))
#define INDEX_FROM_BUF_18(col)    ((((col) & 0xFC0000) >> 6)  + (((col) & 0xFC00) >> 4)  + (((col) & 0xFC) >> 2))
#define INDEX_FROM_BUF_24(col)      ((col) & 0xFFFFFF)

int GPU_select_to_index(unsigned int col)
{
	if (col == 0) {
		return 0;
	}

	switch (GPU_color_depth()) {
		case  8: return INDEX_FROM_BUF_8(col);
		case 12: return INDEX_FROM_BUF_12(col);
		case 15:
		case 16: return INDEX_FROM_BUF_15_16(col);
		case 24: return INDEX_FROM_BUF_24(col);
		default: return INDEX_FROM_BUF_18(col);
	}
}

void GPU_select_to_index_array(unsigned int *col, const unsigned int size)
{
#define INDEX_BUF_ARRAY(INDEX_FROM_BUF_BITS) \
	for (i = size; i--; col++) { \
		if ((c = *col)) { \
			*col = INDEX_FROM_BUF_BITS(c); \
		} \
	} ((void)0)

	if (size > 0) {
		unsigned int i, c;

		switch (GPU_color_depth()) {
			case  8:
				INDEX_BUF_ARRAY(INDEX_FROM_BUF_8);
				break;
			case 12:
				INDEX_BUF_ARRAY(INDEX_FROM_BUF_12);
				break;
			case 15:
			case 16:
				INDEX_BUF_ARRAY(INDEX_FROM_BUF_15_16);
				break;
			case 24:
				INDEX_BUF_ARRAY(INDEX_FROM_BUF_24);
				break;
			default:
				INDEX_BUF_ARRAY(INDEX_FROM_BUF_18);
				break;
		}
	}

#undef INDEX_BUF_ARRAY
}

#define STATE_STACK_DEPTH 16

typedef struct {
	eGPUAttribMask mask;

	/* GL_ENABLE_BIT */
	unsigned int is_blend : 1;
	unsigned int is_cull_face : 1;
	unsigned int is_depth_test : 1;
	unsigned int is_dither : 1;
	unsigned int is_lighting : 1;
	unsigned int is_line_smooth : 1;
	unsigned int is_color_logic_op : 1;
	unsigned int is_multisample : 1;
	unsigned int is_polygon_offset_line : 1;
	unsigned int is_polygon_offset_fill : 1;
	unsigned int is_polygon_smooth : 1;
	unsigned int is_sample_alpha_to_coverage : 1;
	unsigned int is_scissor_test : 1;
	unsigned int is_stencil_test : 1;

	bool is_clip_plane[6];

	/* GL_DEPTH_BUFFER_BIT */
	/* unsigned int is_depth_test : 1; */
	int depth_func;
	double depth_clear_value;
	bool depth_write_mask;

	/* GL_SCISSOR_BIT */
	int scissor_box[4];
	/* unsigned int is_scissor_test : 1; */

	/* GL_VIEWPORT_BIT */
	int viewport[4];
	double near_far[2];
}  GPUAttribValues;

typedef struct {
	GPUAttribValues attrib_stack[STATE_STACK_DEPTH];
	unsigned int top;
} GPUAttribStack;

static GPUAttribStack state = {
	.top = 0
};

#define AttribStack state
#define Attrib state.attrib_stack[state.top]

/**
 * Replacement for glPush/PopAttributes
 *
 * We don't need to cover all the options of legacy OpenGL
 * but simply the ones used by Blender.
 */
void gpuPushAttrib(eGPUAttribMask mask)
{
	Attrib.mask = mask;

	if ((mask & GPU_DEPTH_BUFFER_BIT) != 0) {
		Attrib.is_depth_test = glIsEnabled(GL_DEPTH_TEST);
		glGetIntegerv(GL_DEPTH_FUNC, &Attrib.depth_func);
		glGetDoublev(GL_DEPTH_CLEAR_VALUE, &Attrib.depth_clear_value);
		glGetBooleanv(GL_DEPTH_WRITEMASK, (GLboolean *)&Attrib.depth_write_mask);
	}

	if ((mask & GPU_ENABLE_BIT) != 0) {
		Attrib.is_blend = glIsEnabled(GL_BLEND);

		for (int i = 0; i < 6; i++) {
			Attrib.is_clip_plane[i] = glIsEnabled(GL_CLIP_PLANE0 + i);
		}

		Attrib.is_cull_face = glIsEnabled(GL_CULL_FACE);
		Attrib.is_depth_test = glIsEnabled(GL_DEPTH_TEST);
		Attrib.is_dither = glIsEnabled(GL_DITHER);
		Attrib.is_line_smooth = glIsEnabled(GL_LINE_SMOOTH);
		Attrib.is_color_logic_op = glIsEnabled(GL_COLOR_LOGIC_OP);
		Attrib.is_multisample = glIsEnabled(GL_MULTISAMPLE);
		Attrib.is_polygon_offset_line = glIsEnabled(GL_POLYGON_OFFSET_LINE);
		Attrib.is_polygon_offset_fill = glIsEnabled(GL_POLYGON_OFFSET_FILL);
		Attrib.is_polygon_smooth = glIsEnabled(GL_POLYGON_SMOOTH);
		Attrib.is_sample_alpha_to_coverage = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
		Attrib.is_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
		Attrib.is_stencil_test = glIsEnabled(GL_STENCIL_TEST);
	}

	if ((mask & GPU_SCISSOR_BIT) != 0) {
		Attrib.is_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
		glGetIntegerv(GL_SCISSOR_BOX, (GLint *)&Attrib.scissor_box);
	}

	if ((mask & GPU_VIEWPORT_BIT) != 0) {
		glGetDoublev(GL_DEPTH_RANGE, (GLdouble *)&Attrib.near_far);
		glGetIntegerv(GL_VIEWPORT, (GLint *)&Attrib.viewport);
	}

	if ((mask & GPU_BLEND_BIT) != 0) {
		Attrib.is_blend = glIsEnabled(GL_BLEND);
	}

	BLI_assert(AttribStack.top < STATE_STACK_DEPTH);
	AttribStack.top++;
}

static void restore_mask(GLenum cap, const bool value)
{
	if (value) {
		glEnable(cap);
	}
	else {
		glDisable(cap);
	}
}

void gpuPopAttrib(void)
{
	BLI_assert(AttribStack.top > 0);
	AttribStack.top--;

	GLint mask = Attrib.mask;

	if ((mask & GPU_DEPTH_BUFFER_BIT) != 0) {
		restore_mask(GL_DEPTH_TEST, Attrib.is_depth_test);
		glDepthFunc(Attrib.depth_func);
		glClearDepth(Attrib.depth_clear_value);
		glDepthMask(Attrib.depth_write_mask);
	}

	if ((mask & GPU_ENABLE_BIT) != 0) {
		restore_mask(GL_BLEND, Attrib.is_blend);

		for (int i = 0; i < 6; i++) {
			restore_mask(GL_CLIP_PLANE0 + i, Attrib.is_clip_plane[i]);
		}

		restore_mask(GL_CULL_FACE, Attrib.is_cull_face);
		restore_mask(GL_DEPTH_TEST, Attrib.is_depth_test);
		restore_mask(GL_DITHER, Attrib.is_dither);
		restore_mask(GL_LINE_SMOOTH, Attrib.is_line_smooth);
		restore_mask(GL_COLOR_LOGIC_OP, Attrib.is_color_logic_op);
		restore_mask(GL_MULTISAMPLE, Attrib.is_multisample);
		restore_mask(GL_POLYGON_OFFSET_LINE, Attrib.is_polygon_offset_line);
		restore_mask(GL_POLYGON_OFFSET_FILL, Attrib.is_polygon_offset_fill);
		restore_mask(GL_POLYGON_SMOOTH, Attrib.is_polygon_smooth);
		restore_mask(GL_SAMPLE_ALPHA_TO_COVERAGE, Attrib.is_sample_alpha_to_coverage);
		restore_mask(GL_SCISSOR_TEST, Attrib.is_scissor_test);
		restore_mask(GL_STENCIL_TEST, Attrib.is_stencil_test);
	}

	if ((mask & GPU_VIEWPORT_BIT) != 0) {
		glViewport(Attrib.viewport[0], Attrib.viewport[1], Attrib.viewport[2], Attrib.viewport[3]);
		glDepthRange(Attrib.near_far[0], Attrib.near_far[1]);
	}

	if ((mask & GPU_SCISSOR_BIT) != 0) {
		restore_mask(GL_SCISSOR_TEST, Attrib.is_scissor_test);
		glScissor(Attrib.scissor_box[0], Attrib.scissor_box[1], Attrib.scissor_box[2], Attrib.scissor_box[3]);
	}

	if ((mask & GPU_BLEND_BIT) != 0) {
		restore_mask(GL_BLEND, Attrib.is_blend);
	}
}

#undef Attrib
#undef AttribStack

/** \} */
