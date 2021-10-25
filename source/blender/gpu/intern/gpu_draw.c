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
 * These are some obscure rendering functions shared between the
 * game engine and the blender, in this module to avoid duplication
 * and abstract them away from the rest a bit.
 */

#include <string.h>

#include "GPU_glew.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_hash.h"

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
#ifdef WITH_GAMEENGINE
#  include "BKE_object.h"
#endif

#include "GPU_basic_shader.h"
#include "GPU_buffers.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"
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

/* Text Rendering */

static void gpu_mcol(unsigned int ucol)
{
	/* mcol order is swapped */
	const char *cp = (char *)&ucol;
	glColor3ub(cp[3], cp[2], cp[1]);
}

void GPU_render_text(
        MTexPoly *mtexpoly, int mode,
        const char *textstr, int textlen, unsigned int *col,
        const float *v_quad[4], const float *uv_quad[4],
        int glattrib)
{
	if ((mode & GEMAT_TEXT) && (textlen > 0) && mtexpoly->tpage) {
		const float *v1 = v_quad[0];
		const float *v2 = v_quad[1];
		const float *v3 = v_quad[2];
		const float *v4 = v_quad[3];
		Image *ima = (Image *)mtexpoly->tpage;
		const size_t textlen_st = textlen;
		float centerx, centery, sizex, sizey, transx, transy, movex, movey, advance;
		
		/* multiline */
		float line_start = 0.0f, line_height;
		
		if (v4)
			line_height = max_ffff(v1[1], v2[1], v3[1], v4[2]) - min_ffff(v1[1], v2[1], v3[1], v4[2]);
		else
			line_height = max_fff(v1[1], v2[1], v3[1]) - min_fff(v1[1], v2[1], v3[1]);
		line_height *= 1.2f; /* could be an option? */
		/* end multiline */

		
		/* color has been set */
		if (mtexpoly->mode & TF_OBCOL)
			col = NULL;
		else if (!col)
			glColor3f(1.0f, 1.0f, 1.0f);

		glPushMatrix();
		
		/* get the tab width */
		ImBuf *first_ibuf = BKE_image_get_first_ibuf(ima);
		matrixGlyph(first_ibuf, ' ', &centerx, &centery,
			&sizex, &sizey, &transx, &transy, &movex, &movey, &advance);
		
		float advance_tab = advance * 4; /* tab width could also be an option */
		
		
		for (size_t index = 0; index < textlen_st; ) {
			unsigned int character;
			float uv[4][2];

			/* lets calculate offset stuff */
			character = BLI_str_utf8_as_unicode_and_size_safe(textstr + index, &index);
			
			if (character == '\n') {
				glTranslatef(line_start, -line_height, 0.0f);
				line_start = 0.0f;
				continue;
			}
			else if (character == '\t') {
				glTranslatef(advance_tab, 0.0f, 0.0f);
				line_start -= advance_tab; /* so we can go back to the start of the line */
				continue;
				
			}
			else if (character > USHRT_MAX) {
				/* not much we can do here bmfonts take ushort */
				character = '?';
			}
			
			/* space starts at offset 1 */
			/* character = character - ' ' + 1; */
			matrixGlyph(first_ibuf, character, & centerx, &centery,
				&sizex, &sizey, &transx, &transy, &movex, &movey, &advance);

			uv[0][0] = (uv_quad[0][0] - centerx) * sizex + transx;
			uv[0][1] = (uv_quad[0][1] - centery) * sizey + transy;
			uv[1][0] = (uv_quad[1][0] - centerx) * sizex + transx;
			uv[1][1] = (uv_quad[1][1] - centery) * sizey + transy;
			uv[2][0] = (uv_quad[2][0] - centerx) * sizex + transx;
			uv[2][1] = (uv_quad[2][1] - centery) * sizey + transy;
			
			glBegin(GL_POLYGON);
			if (glattrib >= 0) glVertexAttrib2fv(glattrib, uv[0]);
			else glTexCoord2fv(uv[0]);
			if (col) gpu_mcol(col[0]);
			glVertex3f(sizex * v1[0] + movex, sizey * v1[1] + movey, v1[2]);
			
			if (glattrib >= 0) glVertexAttrib2fv(glattrib, uv[1]);
			else glTexCoord2fv(uv[1]);
			if (col) gpu_mcol(col[1]);
			glVertex3f(sizex * v2[0] + movex, sizey * v2[1] + movey, v2[2]);

			if (glattrib >= 0) glVertexAttrib2fv(glattrib, uv[2]);
			else glTexCoord2fv(uv[2]);
			if (col) gpu_mcol(col[2]);
			glVertex3f(sizex * v3[0] + movex, sizey * v3[1] + movey, v3[2]);

			if (v4) {
				uv[3][0] = (uv_quad[3][0] - centerx) * sizex + transx;
				uv[3][1] = (uv_quad[3][1] - centery) * sizey + transy;

				if (glattrib >= 0) glVertexAttrib2fv(glattrib, uv[3]);
				else glTexCoord2fv(uv[3]);
				if (col) gpu_mcol(col[3]);
				glVertex3f(sizex * v4[0] + movex, sizey * v4[1] + movey, v4[2]);
			}
			glEnd();

			glTranslatef(advance, 0.0f, 0.0f);
			line_start -= advance; /* so we can go back to the start of the line */
		}
		glPopMatrix();

		BKE_image_release_ibuf(ima, first_ibuf, NULL);
	}
}

/* Checking powers of two for images since OpenGL ES requires it */

static bool is_power_of_2_resolution(int w, int h)
{
	return is_power_of_2_i(w) && is_power_of_2_i(h);
}

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
	int curtile, tile;
	int curtilemode, tilemode;
	int curtileXRep, tileXRep;
	int curtileYRep, tileYRep;
	Image *ima, *curima;

	/* also controls min/mag filtering */
	bool domipmap;
	/* only use when 'domipmap' is set */
	bool linearmipmap;
	/* store this so that new images created while texture painting won't be set to mipmapped */
	bool texpaint;

	int alphablend;
	float anisotropic;
	int gpu_mipmap;
	MTexPoly *lasttface;
} GTS = {0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 1, 0, 0, -1, 1.0f, 0, NULL};

/* Mipmap settings */

void GPU_set_gpu_mipmapping(int gpu_mipmap)
{
	int old_value = GTS.gpu_mipmap;

	/* only actually enable if it's supported */
	GTS.gpu_mipmap = gpu_mipmap && GLEW_EXT_framebuffer_object;

	if (old_value != GTS.gpu_mipmap) {
		GPU_free_images();
	}
}

static void gpu_generate_mipmap(GLenum target)
{
	const bool is_ati = GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY);
	int target_enabled = 0;

	/* work around bug in ATI driver, need to have GL_TEXTURE_2D enabled
	 * http://www.opengl.org/wiki/Common_Mistakes#Automatic_mipmap_generation */
	if (is_ati) {
		target_enabled = glIsEnabled(target);
		if (!target_enabled)
			glEnable(target);
	}

	/* TODO: simplify when we transition to GL >= 3 */
	if (GLEW_VERSION_3_0 || GLEW_ARB_framebuffer_object)
		glGenerateMipmap(target);
	else if (GLEW_EXT_framebuffer_object)
		glGenerateMipmapEXT(target);

	if (is_ati && !target_enabled)
		glDisable(target);
}

void GPU_set_mipmap(bool mipmap)
{
	if (GTS.domipmap != mipmap) {
		GPU_free_images();
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
void GPU_set_anisotropic(float value)
{
	if (GTS.anisotropic != value) {
		GPU_free_images();

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

static void gpu_make_repbind(Image *ima)
{
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
	if (ibuf == NULL)
		return;

	if (ima->repbind) {
		glDeleteTextures(ima->totbind, (GLuint *)ima->repbind);
		MEM_freeN(ima->repbind);
		ima->repbind = NULL;
		ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
	}

	ima->totbind = ima->xrep * ima->yrep;

	if (ima->totbind > 1) {
		ima->repbind = MEM_callocN(sizeof(int) * ima->totbind, "repbind");
	}

	BKE_image_release_ibuf(ima, ibuf, NULL);
}

static unsigned int *gpu_get_image_bindcode(Image *ima, GLenum textarget)
{
	unsigned int *bind = 0;

	if (textarget == GL_TEXTURE_2D)
		bind = &ima->bindcode[TEXTARGET_TEXTURE_2D];
	else if (textarget == GL_TEXTURE_CUBE_MAP)
		bind = &ima->bindcode[TEXTARGET_TEXTURE_CUBE_MAP];

	return bind;
}

void GPU_clear_tpage(bool force)
{
	if (GTS.lasttface == NULL && !force)
		return;
	
	GTS.lasttface = NULL;
	GTS.curtile = 0;
	GTS.curima = NULL;
	if (GTS.curtilemode != 0) {
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
	}
	GTS.curtilemode = 0;
	GTS.curtileXRep = 0;
	GTS.curtileYRep = 0;
	GTS.alphablend = -1;
	
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glDisable(GL_ALPHA_TEST);
}

static void gpu_set_alpha_blend(GPUBlendMode alphablend)
{
	if (alphablend == GPU_BLEND_SOLID) {
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (alphablend == GPU_BLEND_ADD) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	}
	else if (ELEM(alphablend, GPU_BLEND_ALPHA, GPU_BLEND_ALPHA_SORT)) {
		glEnable(GL_BLEND);
		glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

		/* for OpenGL render we use the alpha channel, this makes alpha blend correct */
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		
		/* if U.glalphaclip == 1.0, some cards go bonkers...
		 * turn off alpha test in this case */

		/* added after 2.45 to clip alpha */
		if (U.glalphaclip == 1.0f) {
			glDisable(GL_ALPHA_TEST);
		}
		else {
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, U.glalphaclip);
		}
	}
	else if (alphablend == GPU_BLEND_CLIP) {
		glDisable(GL_BLEND);
		glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
	}
	else if (alphablend == GPU_BLEND_ALPHA_TO_COVERAGE) {
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, U.glalphaclip);
		glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	}
}

static void gpu_verify_alpha_blend(int alphablend)
{
	/* verify alpha blending modes */
	if (GTS.alphablend == alphablend)
		return;

	gpu_set_alpha_blend(alphablend);
	GTS.alphablend = alphablend;
}

static void gpu_verify_reflection(Image *ima)
{
	if (ima && (ima->flag & IMA_REFLECT)) {
		/* enable reflection mapping */
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);

		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
	}
	else {
		/* disable reflection mapping */
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
	}
}

typedef struct VerifyThreadData {
	ImBuf *ibuf;
	float *srgb_frect;
} VerifyThreadData;

static void gpu_verify_high_bit_srgb_buffer_slice(float *srgb_frect,
                                                  ImBuf *ibuf,
                                                  const int start_line,
                                                  const int height)
{
	size_t offset = ibuf->channels * start_line * ibuf->x;
	float *current_srgb_frect = srgb_frect + offset;
	float *current_rect_float = ibuf->rect_float + offset;
	IMB_buffer_float_from_float(current_srgb_frect,
	                            current_rect_float,
	                            ibuf->channels,
	                            IB_PROFILE_SRGB,
	                            IB_PROFILE_LINEAR_RGB, true,
	                            ibuf->x, height,
	                            ibuf->x, ibuf->x);
	IMB_buffer_float_unpremultiply(current_srgb_frect, ibuf->x, height);
	/* Clamp buffer colors to 1.0 to avoid artifacts due to glu for hdr images. */
	IMB_buffer_float_clamp(current_srgb_frect, ibuf->x, height);
}

static void verify_thread_do(void *data_v,
                             int start_scanline,
                             int num_scanlines)
{
	VerifyThreadData *data = (VerifyThreadData *)data_v;
	gpu_verify_high_bit_srgb_buffer_slice(data->srgb_frect,
	                                      data->ibuf,
	                                      start_scanline,
	                                      num_scanlines);
}

static void gpu_verify_high_bit_srgb_buffer(float *srgb_frect,
                                            ImBuf *ibuf)
{
	if (ibuf->y < 64) {
		gpu_verify_high_bit_srgb_buffer_slice(srgb_frect,
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

int GPU_verify_image(
        Image *ima, ImageUser *iuser,
        int textarget, int tftile, bool compare, bool mipmap, bool is_data)
{
	unsigned int *bind = NULL;
	int tpx = 0, tpy = 0;
	unsigned int *rect = NULL;
	float *frect = NULL;
	float *srgb_frect = NULL;
	/* flag to determine whether deep format is used */
	bool use_high_bit_depth = false, do_color_management = false;

	/* initialize tile mode and number of repeats */
	GTS.ima = ima;
	GTS.tilemode = (ima && (ima->tpageflag & (IMA_TILES | IMA_TWINANIM)));
	GTS.tileXRep = 0;
	GTS.tileYRep = 0;

	/* setting current tile according to frame */
	if (ima && (ima->tpageflag & IMA_TWINANIM))
		GTS.tile = ima->lastframe;
	else
		GTS.tile = tftile;

	GTS.tile = MAX2(0, GTS.tile);

	if (ima) {
		GTS.tileXRep = ima->xrep;
		GTS.tileYRep = ima->yrep;
	}

	/* if same image & tile, we're done */
	if (compare && ima == GTS.curima && GTS.curtile == GTS.tile &&
	    GTS.tilemode == GTS.curtilemode && GTS.curtileXRep == GTS.tileXRep &&
	    GTS.curtileYRep == GTS.tileYRep)
	{
		return (ima != NULL);
	}

	/* if tiling mode or repeat changed, change texture matrix to fit */
	if (GTS.tilemode != GTS.curtilemode || GTS.curtileXRep != GTS.tileXRep ||
	    GTS.curtileYRep != GTS.tileYRep)
	{
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		if (ima && (ima->tpageflag & IMA_TILES))
			glScalef(ima->xrep, ima->yrep, 1.0f);

		glMatrixMode(GL_MODELVIEW);
	}

	/* check if we have a valid image */
	if (ima == NULL || ima->ok == 0)
		return 0;

	/* check if we have a valid image buffer */
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);

	if (ibuf == NULL)
		return 0;

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

	/* currently, tpage refresh is used by ima sequences */
	if (ima->tpageflag & IMA_TPAGE_REFRESH) {
		GPU_free_image(ima);
		ima->tpageflag &= ~IMA_TPAGE_REFRESH;
	}
	
	if (GTS.tilemode) {
		/* tiled mode */
		if (ima->repbind == NULL) gpu_make_repbind(ima);
		if (GTS.tile >= ima->totbind) GTS.tile = 0;
		
		/* this happens when you change repeat buttons */
		if (ima->repbind && textarget == GL_TEXTURE_2D) bind = &ima->repbind[GTS.tile];
		else bind = gpu_get_image_bindcode(ima, textarget);
		
		if (*bind == 0) {
			short texwindx = ibuf->x / ima->xrep;
			short texwindy = ibuf->y / ima->yrep;
			
			if (GTS.tile >= ima->xrep * ima->yrep)
				GTS.tile = ima->xrep * ima->yrep - 1;
	
			short texwinsy = GTS.tile / ima->xrep;
			short texwinsx = GTS.tile - texwinsy * ima->xrep;
	
			texwinsx *= texwindx;
			texwinsy *= texwindy;
	
			tpx = texwindx;
			tpy = texwindy;

			if (use_high_bit_depth) {
				if (do_color_management) {
					srgb_frect = MEM_mallocN(ibuf->x * ibuf->y * sizeof(float) * 4, "floar_buf_col_cor");
					gpu_verify_high_bit_srgb_buffer(srgb_frect, ibuf);
					frect = srgb_frect + texwinsy * ibuf->x + texwinsx;
				}
				else {
					frect = ibuf->rect_float + texwinsy * ibuf->x + texwinsx;
				}
			}
			else {
				rect = ibuf->rect + texwinsy * ibuf->x + texwinsx;
			}
		}
	}
	else {
		/* regular image mode */
		bind = gpu_get_image_bindcode(ima, textarget);

		if (*bind == 0) {
			tpx = ibuf->x;
			tpy = ibuf->y;
			rect = ibuf->rect;
			if (use_high_bit_depth) {
				if (do_color_management) {
					frect = srgb_frect = MEM_mallocN(ibuf->x * ibuf->y * sizeof(*srgb_frect) * 4, "floar_buf_col_cor");
					gpu_verify_high_bit_srgb_buffer(srgb_frect, ibuf);
				}
				else
					frect = ibuf->rect_float;
			}
		}
	}

	if (*bind != 0) {
		/* enable opengl drawing with textures */
		glBindTexture(textarget, *bind);
		BKE_image_release_ibuf(ima, ibuf, NULL);
		return *bind;
	}

	const int rectw = tpx;
	const int recth = tpy;

	unsigned *tilerect = NULL;
	float *ftilerect = NULL;

	/* for tiles, copy only part of image into buffer */
	if (GTS.tilemode) {
		if (use_high_bit_depth) {
			ftilerect = MEM_mallocN(rectw * recth * sizeof(*ftilerect), "tilerect");

			for (int y = 0; y < recth; y++) {
				const float *frectrow = &frect[y * ibuf->x];
				float *ftilerectrow = &ftilerect[y * rectw];

				memcpy(ftilerectrow, frectrow, tpx * sizeof(*frectrow));
			}

			frect = ftilerect;
		}
		else {
			tilerect = MEM_mallocN(rectw * recth * sizeof(*tilerect), "tilerect");

			for (int y = 0; y < recth; y++) {
				const unsigned *rectrow = &rect[y * ibuf->x];
				unsigned *tilerectrow = &tilerect[y * rectw];

				memcpy(tilerectrow, rectrow, tpx * sizeof(*rectrow));
			}
			
			rect = tilerect;
		}
	}

#ifdef WITH_DDS
	if (ibuf->ftype == IMB_FTYPE_DDS)
		GPU_create_gl_tex_compressed(bind, rect, rectw, recth, textarget, mipmap, ima, ibuf);
	else
#endif
		GPU_create_gl_tex(bind, rect, frect, rectw, recth, textarget, mipmap, use_high_bit_depth, ima);
	
	/* mark as non-color data texture */
	if (*bind) {
		if (is_data)
			ima->tpageflag |= IMA_GLBIND_IS_DATA;	
		else
			ima->tpageflag &= ~IMA_GLBIND_IS_DATA;	
	}

	/* clean up */
	if (tilerect)
		MEM_freeN(tilerect);
	if (ftilerect)
		MEM_freeN(ftilerect);
	if (srgb_frect)
		MEM_freeN(srgb_frect);

	BKE_image_release_ibuf(ima, ibuf, NULL);

	return *bind;
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

	/* scale if not a power of two. this is not strictly necessary for newer
	 * GPUs (OpenGL version >= 2.0) since they support non-power-of-two-textures 
	 * Then don't bother scaling for hardware that supports NPOT textures! */
	if (textarget == GL_TEXTURE_2D &&
	    ((!GPU_full_non_power_of_two_support() && !is_power_of_2_resolution(rectw, recth)) ||
	     is_over_resolution_limit(textarget, rectw, recth)))
	{
		rectw = smaller_power_of_2_limit(rectw);
		recth = smaller_power_of_2_limit(recth);

		if (use_high_bit_depth) {
			ibuf = IMB_allocFromBuffer(NULL, frect, tpx, tpy);
			IMB_scaleImBuf(ibuf, rectw, recth);

			frect = ibuf->rect_float;
		}
		else {
			ibuf = IMB_allocFromBuffer(rect, NULL, tpx, tpy);
			IMB_scaleImBuf(ibuf, rectw, recth);

			rect = ibuf->rect;
		}
	}

	/* create image */
	glGenTextures(1, (GLuint *)bind);
	glBindTexture(textarget, *bind);

	if (textarget == GL_TEXTURE_2D) {
		if (use_high_bit_depth) {
			if (GLEW_ARB_texture_float)
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, rectw, recth, 0, GL_RGBA, GL_FLOAT, frect);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, rectw, recth, 0, GL_RGBA, GL_FLOAT, frect);
		}
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rectw, recth, 0, GL_RGBA, GL_UNSIGNED_BYTE, rect);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));

		if (GPU_get_mipmap() && mipmap) {
			if (GTS.gpu_mipmap) {
				gpu_generate_mipmap(GL_TEXTURE_2D);
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
							glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA16F_ARB, mip->x, mip->y, 0, GL_RGBA, GL_FLOAT, mip->rect_float);
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
			GLenum informat = use_high_bit_depth ? (GLEW_ARB_texture_float ? GL_RGBA16F_ARB : GL_RGBA16) : GL_RGBA8;
			GLenum type = use_high_bit_depth ? GL_FLOAT : GL_UNSIGNED_BYTE;

			if (cube_map)
				for (int i = 0; i < 6; i++)
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, informat, w, h, 0, GL_RGBA, type, cube_map[i]);

			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));

			if (GPU_get_mipmap() && mipmap) {
				if (GTS.gpu_mipmap) {
					gpu_generate_mipmap(GL_TEXTURE_CUBE_MAP);
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
								glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, i,
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

		glCompressedTexImage2D(GL_TEXTURE_2D, i, format, width, height,
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
#endif
}
static void gpu_verify_repeat(Image *ima)
{
	/* set either clamp or repeat in X/Y */
	if (ima->tpageflag & IMA_CLAMP_U)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

	if (ima->tpageflag & IMA_CLAMP_V)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

int GPU_set_tpage(MTexPoly *mtexpoly, int mipmap, int alphablend)
{
	/* check if we need to clear the state */
	if (mtexpoly == NULL) {
		GPU_clear_tpage(false);
		return 0;
	}

	Image *ima = mtexpoly->tpage;
	GTS.lasttface = mtexpoly;

	gpu_verify_alpha_blend(alphablend);
	gpu_verify_reflection(ima);

	if (GPU_verify_image(ima, NULL, GL_TEXTURE_2D, mtexpoly->tile, 1, mipmap, false)) {
		GTS.curtile = GTS.tile;
		GTS.curima = GTS.ima;
		GTS.curtilemode = GTS.tilemode;
		GTS.curtileXRep = GTS.tileXRep;
		GTS.curtileYRep = GTS.tileYRep;

		glEnable(GL_TEXTURE_2D);
	}
	else {
		glDisable(GL_TEXTURE_2D);
		
		GTS.curtile = 0;
		GTS.curima = NULL;
		GTS.curtilemode = 0;
		GTS.curtileXRep = 0;
		GTS.curtileYRep = 0;

		return 0;
	}
	
	gpu_verify_repeat(ima);
	
	/* Did this get lost in the image recode? */
	/* BKE_image_tag_time(ima);*/

	return 1;
}

/* these two functions are called on entering and exiting texture paint mode,
 * temporary disabling/enabling mipmapping on all images for quick texture
 * updates with glTexSubImage2D. images that didn't change don't have to be
 * re-uploaded to OpenGL */
void GPU_paint_set_mipmap(bool mipmap)
{
	if (!GTS.domipmap)
		return;

	GTS.texpaint = !mipmap;

	if (mipmap) {
		for (Image *ima = G.main->image.first; ima; ima = ima->id.next) {
			if (BKE_image_has_bindcode(ima)) {
				if (ima->tpageflag & IMA_MIPMAP_COMPLETE) {
					if (ima->bindcode[TEXTARGET_TEXTURE_2D]) {
						glBindTexture(GL_TEXTURE_2D, ima->bindcode[TEXTARGET_TEXTURE_2D]);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gpu_get_mipmap_filter(0));
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));
					}
					if (ima->bindcode[TEXTARGET_TEXTURE_CUBE_MAP]) {
						glBindTexture(GL_TEXTURE_CUBE_MAP, ima->bindcode[TEXTARGET_TEXTURE_CUBE_MAP]);
						glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, gpu_get_mipmap_filter(0));
						glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));
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
		for (Image *ima = G.main->image.first; ima; ima = ima->id.next) {
			if (BKE_image_has_bindcode(ima)) {
				if (ima->bindcode[TEXTARGET_TEXTURE_2D]) {
					glBindTexture(GL_TEXTURE_2D, ima->bindcode[TEXTARGET_TEXTURE_2D]);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));
				}
				if (ima->bindcode[TEXTARGET_TEXTURE_CUBE_MAP]) {
					glBindTexture(GL_TEXTURE_CUBE_MAP, ima->bindcode[TEXTARGET_TEXTURE_CUBE_MAP]);
					glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));
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
	if ((!GPU_full_non_power_of_two_support() && !is_power_of_2_resolution(ibuf->x, ibuf->y)) ||
	    is_over_resolution_limit(GL_TEXTURE_2D, ibuf->x, ibuf->y))
	{
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

		/* float rectangles are already continuous in memory so we can use IMB_scaleImBuf */
		if (frect) {
			ImBuf *ibuf_scale = IMB_allocFromBuffer(NULL, frect, w, h);
			IMB_scaleImBuf(ibuf_scale, rectw, recth);

			glBindTexture(GL_TEXTURE_2D, ima->bindcode[TEXTARGET_TEXTURE_2D]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, rectw, recth, GL_RGBA,
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
			glBindTexture(GL_TEXTURE_2D, ima->bindcode[TEXTARGET_TEXTURE_2D]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, rectw, recth, GL_RGBA,
			                GL_UNSIGNED_BYTE, scalerect);

			MEM_freeN(scalerect);
		}

		if (GPU_get_mipmap()) {
			gpu_generate_mipmap(GL_TEXTURE_2D);
		}
		else {
			ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
		}

		return true;
	}

	return false;
}

void GPU_paint_update_image(Image *ima, ImageUser *iuser, int x, int y, int w, int h)
{
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);

	if (ima->repbind ||
	    (!GTS.gpu_mipmap && GPU_get_mipmap()) ||
	    (ima->bindcode[TEXTARGET_TEXTURE_2D] == 0) ||
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

			glBindTexture(GL_TEXTURE_2D, ima->bindcode[TEXTARGET_TEXTURE_2D]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_FLOAT, buffer);

			MEM_freeN(buffer);

			/* we have already accounted for the case where GTS.gpu_mipmap is false
			 * so we will be using GPU mipmap generation here */
			if (GPU_get_mipmap()) {
				gpu_generate_mipmap(GL_TEXTURE_2D);
			}
			else {
				ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
			}

			BKE_image_release_ibuf(ima, ibuf, NULL);
			return;
		}

		if (gpu_check_scaled_image(ibuf, ima, NULL, x, y, w, h)) {
			BKE_image_release_ibuf(ima, ibuf, NULL);
			return;
		}

		glBindTexture(GL_TEXTURE_2D, ima->bindcode[TEXTARGET_TEXTURE_2D]);

		glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
		glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
		glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, ibuf->x);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA,
			GL_UNSIGNED_BYTE, ibuf->rect);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);

		/* see comment above as to why we are using gpu mipmap generation here */
		if (GPU_get_mipmap()) {
			gpu_generate_mipmap(GL_TEXTURE_2D);
		}
		else {
			ima->tpageflag &= ~IMA_MIPMAP_COMPLETE;
		}
	}

	BKE_image_release_ibuf(ima, ibuf, NULL);
}

void GPU_update_images_framechange(void)
{
	for (Image *ima = G.main->image.first; ima; ima = ima->id.next) {
		if (ima->tpageflag & IMA_TWINANIM) {
			if (ima->twend >= ima->xrep * ima->yrep)
				ima->twend = ima->xrep * ima->yrep - 1;
		
			/* check: is bindcode not in the array? free. (to do) */
			
			ima->lastframe++;
			if (ima->lastframe > ima->twend)
				ima->lastframe = ima->twsta;
		}
	}
}

int GPU_update_image_time(Image *ima, double time)
{
	if (!ima)
		return 0;

	if (ima->lastupdate < 0)
		ima->lastupdate = 0;

	if (ima->lastupdate > (float)time)
		ima->lastupdate = (float)time;

	int inc = 0;

	if (ima->tpageflag & IMA_TWINANIM) {
		if (ima->twend >= ima->xrep * ima->yrep) ima->twend = ima->xrep * ima->yrep - 1;
		
		/* check: is the bindcode not in the array? Then free. (still to do) */
		
		float diff = (float)((float)time - ima->lastupdate);
		inc = (int)(diff * (float)ima->animspeed);

		ima->lastupdate += ((float)inc / (float)ima->animspeed);

		int newframe = ima->lastframe + inc;

		if (newframe > (int)ima->twend) {
			if (ima->twend - ima->twsta != 0)
				newframe = (int)ima->twsta - 1 + (newframe - ima->twend) % (ima->twend - ima->twsta);
			else
				newframe = ima->twsta;
		}

		ima->lastframe = newframe;
	}

	return inc;
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
				sds->tex = GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], 4, data);
				MEM_freeN(data);
			}
			/* density only */
			else {
				sds->tex = GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], 1, smoke_get_density(sds->fluid));
			}
			sds->tex_flame = (smoke_has_fuel(sds->fluid)) ? GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], 1, smoke_get_flame(sds->fluid)) : NULL;
		}
		else if (!sds->tex && highres) {
			/* rgba texture for color + density */
			if (smoke_turbulence_has_colors(sds->wt)) {
				float *data = MEM_callocN(sizeof(float) * smoke_turbulence_get_cells(sds->wt) * 4, "smokeColorTexture");
				smoke_turbulence_get_rgba(sds->wt, data, 0);
				sds->tex = GPU_texture_create_3D(sds->res_wt[0], sds->res_wt[1], sds->res_wt[2], 4, data);
				MEM_freeN(data);
			}
			/* density only */
			else {
				sds->tex = GPU_texture_create_3D(sds->res_wt[0], sds->res_wt[1], sds->res_wt[2], 1, smoke_turbulence_get_density(sds->wt));
			}
			sds->tex_flame = (smoke_turbulence_has_fuel(sds->wt)) ? GPU_texture_create_3D(sds->res_wt[0], sds->res_wt[1], sds->res_wt[2], 1, smoke_turbulence_get_flame(sds->wt)) : NULL;
		}

		sds->tex_shadow = GPU_texture_create_3D(sds->res[0], sds->res[1], sds->res[2], 1, sds->shadow);
	}
#else // WITH_SMOKE
	(void)highres;
	smd->domain->tex = NULL;
	smd->domain->tex_flame = NULL;
	smd->domain->tex_shadow = NULL;
#endif // WITH_SMOKE
}

static LinkNode *image_free_queue = NULL;

static void gpu_queue_image_for_free(Image *ima)
{
	BLI_lock_thread(LOCK_OPENGL);
	BLI_linklist_prepend(&image_free_queue, ima);
	BLI_unlock_thread(LOCK_OPENGL);
}

void GPU_free_unused_buffers(void)
{
	if (!BLI_thread_is_main())
		return;

	BLI_lock_thread(LOCK_OPENGL);

	/* images */
	for (LinkNode *node = image_free_queue; node; node = node->next) {
		Image *ima = node->link;

		/* check in case it was freed in the meantime */
		if (G.main && BLI_findindex(&G.main->image, ima) != -1)
			GPU_free_image(ima);
	}

	BLI_linklist_free(image_free_queue, NULL);
	image_free_queue = NULL;

	/* vbo buffers */
	GPU_global_buffer_pool_free_unused();

	BLI_unlock_thread(LOCK_OPENGL);
}

void GPU_free_image(Image *ima)
{
	if (!BLI_thread_is_main()) {
		gpu_queue_image_for_free(ima);
		return;
	}

	for (int i = 0; i < TEXTARGET_COUNT; i++) {
		/* free regular image binding */
		if (ima->bindcode[i]) {
			glDeleteTextures(1, (GLuint *)&ima->bindcode[i]);
			ima->bindcode[i] = 0;
		}
		/* free glsl image binding */
		if (ima->gputexture[i]) {
			GPU_texture_free(ima->gputexture[i]);
			ima->gputexture[i] = NULL;
		}
	}

	/* free repeated image binding */
	if (ima->repbind) {
		glDeleteTextures(ima->totbind, (GLuint *)ima->repbind);
	
		MEM_freeN(ima->repbind);
		ima->repbind = NULL;
	}

	ima->tpageflag &= ~(IMA_MIPMAP_COMPLETE | IMA_GLBIND_IS_DATA);
}

void GPU_free_images(void)
{
	if (G.main)
		for (Image *ima = G.main->image.first; ima; ima = ima->id.next)
			GPU_free_image(ima);
}

/* same as above but only free animated images */
void GPU_free_images_anim(void)
{
	if (G.main)
		for (Image *ima = G.main->image.first; ima; ima = ima->id.next)
			if (BKE_image_is_animated(ima))
				GPU_free_image(ima);
}


void GPU_free_images_old(void)
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

	Image *ima = G.main->image.first;
	while (ima) {
		if ((ima->flag & IMA_NOCOLLECT) == 0 && ctime - ima->lastused > U.textimeout) {
			/* If it's in GL memory, deallocate and set time tag to current time
			 * This gives textures a "second chance" to be used before dying. */
			if (BKE_image_has_bindcode(ima) || ima->repbind) {
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


/* OpenGL Materials */

#define FIXEDMAT 8

/* OpenGL state caching for materials */

typedef struct GPUMaterialFixed {
	float diff[3];
	float spec[3];
	int hard;
	float alpha;
} GPUMaterialFixed; 

static struct GPUMaterialState {
	GPUMaterialFixed (*matbuf);
	GPUMaterialFixed matbuf_fixed[FIXEDMAT];
	int totmat;

	/* set when called inside GPU_begin_object_materials / GPU_end_object_materials
	 * otherwise calling GPU_object_material_bind returns zero */
	bool is_enabled;

	Material **gmatbuf;
	Material *gmatbuf_fixed[FIXEDMAT];
	Material *gboundmat;
	Object *gob;
	DupliObject *dob;
	Scene *gscene;
	int glay;
	bool gscenelock;
	float (*gviewmat)[4];
	float (*gviewinv)[4];
	float (*gviewcamtexcofac);

	bool backface_culling;
	bool two_sided_lighting;

	GPUBlendMode *alphablend;
	GPUBlendMode alphablend_fixed[FIXEDMAT];
	bool use_alpha_pass, is_alpha_pass;
	bool use_matcaps;

	int lastmatnr, lastretval;
	GPUBlendMode lastalphablend;
	bool is_opensubdiv;
} GMS = {NULL};

/* fixed function material, alpha handed by caller */
static void gpu_material_to_fixed(
        GPUMaterialFixed *smat, const Material *bmat, const int gamma, const Object *ob,
        const int new_shading_nodes, const bool dimdown)
{
	if (bmat->mode & MA_SHLESS) {
		copy_v3_v3(smat->diff, &bmat->r);

		if (gamma)
			linearrgb_to_srgb_v3_v3(smat->diff, smat->diff);

		zero_v3(smat->spec);
		smat->alpha = 1.0f;
		smat->hard = 0;
	}
	else if (new_shading_nodes) {
		copy_v3_v3(smat->diff, &bmat->r);
		copy_v3_v3(smat->spec, &bmat->specr);
		smat->alpha = 1.0f;
		smat->hard = CLAMPIS(bmat->har, 0, 128);
		
		if (dimdown) {
			mul_v3_fl(smat->diff, 0.8f);
			mul_v3_fl(smat->spec, 0.5f);
		}
		
		if (gamma) {
			linearrgb_to_srgb_v3_v3(smat->diff, smat->diff);
			linearrgb_to_srgb_v3_v3(smat->spec, smat->spec);
		}
	}
	else {
		mul_v3_v3fl(smat->diff, &bmat->r, bmat->ref + bmat->emit);

		if (bmat->shade_flag & MA_OBCOLOR)
			mul_v3_v3(smat->diff, ob->col);
		
		mul_v3_v3fl(smat->spec, &bmat->specr, bmat->spec);
		smat->hard = CLAMPIS(bmat->har, 1, 128);
		smat->alpha = 1.0f;

		if (gamma) {
			linearrgb_to_srgb_v3_v3(smat->diff, smat->diff);
			linearrgb_to_srgb_v3_v3(smat->spec, smat->spec);
		}
	}
}

static Material *gpu_active_node_material(Material *ma)
{
	if (ma && ma->use_nodes && ma->nodetree) {
		bNode *node = nodeGetActiveID(ma->nodetree, ID_MA);

		if (node)
			return (Material *)node->id;
		else
			return NULL;
	}

	return ma;
}

void GPU_begin_dupli_object(DupliObject *dob)
{
	GMS.dob = dob;
}

void GPU_end_dupli_object(void)
{
	GMS.dob = NULL;
}

void GPU_begin_object_materials(
        View3D *v3d, RegionView3D *rv3d, Scene *scene, Object *ob,
        bool glsl, bool *do_alpha_after)
{
	Material *ma;
	GPUMaterial *gpumat;
	GPUBlendMode alphablend;
	DupliObject *dob;
	int a;
	const bool gamma = BKE_scene_check_color_management_enabled(scene);
	const bool new_shading_nodes = BKE_scene_use_new_shading_nodes(scene);
	const bool use_matcap = (v3d->flag2 & V3D_SHOW_SOLID_MATCAP) != 0;  /* assumes v3d->defmaterial->preview is set */
	bool use_opensubdiv = false;

#ifdef WITH_OPENSUBDIV
	{
		DerivedMesh *derivedFinal = NULL;
		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			BMEditMesh *em = me->edit_btmesh;
			if (em != NULL) {
				derivedFinal = em->derivedFinal;
			}
			else {
				derivedFinal = ob->derivedFinal;
			}
		}
		else {
			derivedFinal = ob->derivedFinal;
		}

		if (derivedFinal != NULL && derivedFinal->type == DM_TYPE_CCGDM) {
			CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) derivedFinal;
			use_opensubdiv = ccgdm->useGpuBackend;
		}
	}
#endif

#ifdef WITH_GAMEENGINE
	if (rv3d->rflag & RV3D_IS_GAME_ENGINE) {
		ob = BKE_object_lod_matob_get(ob, scene);
	}
#endif

	/* initialize state */
	/* DupliObject must be restored */
	dob = GMS.dob;
	memset(&GMS, 0, sizeof(GMS));
	GMS.is_enabled = true;
	GMS.dob = dob;
	GMS.lastmatnr = -1;
	GMS.lastretval = -1;
	GMS.lastalphablend = GPU_BLEND_SOLID;
	GMS.use_matcaps = use_matcap;

	GMS.backface_culling = (v3d->flag2 & V3D_BACKFACE_CULLING) != 0;

	GMS.two_sided_lighting = false;
	if (ob && ob->type == OB_MESH)
		GMS.two_sided_lighting = (((Mesh *)ob->data)->flag & ME_TWOSIDED) != 0;

	GMS.gob = ob;
	GMS.gscene = scene;
	GMS.is_opensubdiv = use_opensubdiv;
	GMS.totmat = use_matcap ? 1 : ob->totcol + 1;  /* materials start from 1, default material is 0 */
	GMS.glay = (v3d->localvd) ? v3d->localvd->lay : v3d->lay; /* keep lamps visible in local view */
	GMS.gscenelock = (v3d->scenelock != 0);
	GMS.gviewmat = rv3d->viewmat;
	GMS.gviewinv = rv3d->viewinv;
	GMS.gviewcamtexcofac = rv3d->viewcamtexcofac;

	/* alpha pass setup. there's various cases to handle here:
	 * - object transparency on: only solid materials draw in the first pass,
	 * and only transparent in the second 'alpha' pass.
	 * - object transparency off: for glsl we draw both in a single pass, and
	 * for solid we don't use transparency at all. */
	GMS.use_alpha_pass = (do_alpha_after != NULL);
	GMS.is_alpha_pass = (v3d->transp != false);
	if (GMS.use_alpha_pass)
		*do_alpha_after = false;
	
	if (GMS.totmat > FIXEDMAT) {
		GMS.matbuf = MEM_callocN(sizeof(GPUMaterialFixed) * GMS.totmat, "GMS.matbuf");
		GMS.gmatbuf = MEM_callocN(sizeof(*GMS.gmatbuf) * GMS.totmat, "GMS.matbuf");
		GMS.alphablend = MEM_callocN(sizeof(*GMS.alphablend) * GMS.totmat, "GMS.matbuf");
	}
	else {
		GMS.matbuf = GMS.matbuf_fixed;
		GMS.gmatbuf = GMS.gmatbuf_fixed;
		GMS.alphablend = GMS.alphablend_fixed;
	}

	/* viewport material, setup in space_view3d, defaults to matcap using ma->preview now */
	if (use_matcap) {
		GMS.gmatbuf[0] = v3d->defmaterial;
		GPU_material_matcap(scene, v3d->defmaterial, use_opensubdiv);

		/* do material 1 too, for displists! */
		memcpy(&GMS.matbuf[1], &GMS.matbuf[0], sizeof(GPUMaterialFixed));
	
		GMS.alphablend[0] = GPU_BLEND_SOLID;
	}
	else {
	
		/* no materials assigned? */
		if (ob->totcol == 0) {
			gpu_material_to_fixed(&GMS.matbuf[0], &defmaterial, 0, ob, new_shading_nodes, true);

			/* do material 1 too, for displists! */
			memcpy(&GMS.matbuf[1], &GMS.matbuf[0], sizeof(GPUMaterialFixed));

			if (glsl) {
				GMS.gmatbuf[0] = &defmaterial;
				GPU_material_from_blender(GMS.gscene, &defmaterial, GMS.is_opensubdiv);
			}

			GMS.alphablend[0] = GPU_BLEND_SOLID;
		}
		
		/* setup materials */
		for (a = 1; a <= ob->totcol; a++) {
			/* find a suitable material */
			ma = give_current_material(ob, a);
			if (!glsl && !new_shading_nodes) ma = gpu_active_node_material(ma);
			if (ma == NULL) ma = &defmaterial;

			/* create glsl material if requested */
			gpumat = glsl ? GPU_material_from_blender(GMS.gscene, ma, GMS.is_opensubdiv) : NULL;

			if (gpumat) {
				/* do glsl only if creating it succeed, else fallback */
				GMS.gmatbuf[a] = ma;
				alphablend = GPU_material_alpha_blend(gpumat, ob->col);
			}
			else {
				/* fixed function opengl materials */
				gpu_material_to_fixed(&GMS.matbuf[a], ma, gamma, ob, new_shading_nodes, false);

				if (GMS.use_alpha_pass && ((ma->mode & MA_TRANSP) || (new_shading_nodes && ma->alpha != 1.0f))) {
					GMS.matbuf[a].alpha = ma->alpha;
					alphablend = (ma->alpha == 1.0f) ? GPU_BLEND_SOLID: GPU_BLEND_ALPHA;
				}
				else {
					GMS.matbuf[a].alpha = 1.0f;
					alphablend = GPU_BLEND_SOLID;
				}
			}

			/* setting 'do_alpha_after = true' indicates this object needs to be
			 * drawn in a second alpha pass for improved blending */
			if (do_alpha_after && !GMS.is_alpha_pass)
				if (ELEM(alphablend, GPU_BLEND_ALPHA, GPU_BLEND_ADD, GPU_BLEND_ALPHA_SORT))
					*do_alpha_after = true;

			GMS.alphablend[a] = alphablend;
		}
	}

	/* let's start with a clean state */
	GPU_object_material_unbind();
}

static int gpu_get_particle_info(GPUParticleInfo *pi)
{
	DupliObject *dob = GMS.dob;
	if (dob->particle_system) {
		int ind;
		if (dob->persistent_id[0] < dob->particle_system->totpart)
			ind = dob->persistent_id[0];
		else {
			ind = dob->particle_system->child[dob->persistent_id[0] - dob->particle_system->totpart].parent;
		}
		if (ind >= 0) {
			ParticleData *p = &dob->particle_system->particles[ind];

			pi->scalprops[0] = ind;
			pi->scalprops[1] = GMS.gscene->r.cfra - p->time;
			pi->scalprops[2] = p->lifetime;
			pi->scalprops[3] = p->size;

			copy_v3_v3(pi->location, p->state.co);
			copy_v3_v3(pi->velocity, p->state.vel);
			copy_v3_v3(pi->angular_velocity, p->state.ave);
			return 1;
		}
		else return 0;
	}
	else
		return 0;
}

static void GPU_get_object_info(float oi[3], Material *mat)
{
	Object *ob = GMS.gob;
	oi[0] = ob->index;
	oi[1] = mat->index;
	unsigned int random;
	if (GMS.dob) {
		random = GMS.dob->random_id;
	}
	else {
		random = BLI_hash_int_2d(BLI_hash_string(GMS.gob->id.name + 2), 0);
	}
	oi[2] = random * (1.0f / (float)0xFFFFFFFF);
}

int GPU_object_material_bind(int nr, void *attribs)
{
	GPUVertexAttribs *gattribs = attribs;

	/* no GPU_begin_object_materials, use default material */
	if (!GMS.matbuf) {
		memset(&GMS, 0, sizeof(GMS));

		float diffuse[3], specular[3];
		mul_v3_v3fl(diffuse, &defmaterial.r, defmaterial.ref + defmaterial.emit);
		mul_v3_v3fl(specular, &defmaterial.specr, defmaterial.spec);
		GPU_basic_shader_colors(diffuse, specular, 35, 1.0f);

		if (GMS.two_sided_lighting)
			GPU_basic_shader_bind(GPU_SHADER_LIGHTING | GPU_SHADER_TWO_SIDED);
		else
			GPU_basic_shader_bind(GPU_SHADER_LIGHTING);

		return 0;
	}

	/* prevent index to use un-initialized array items */
	if (nr >= GMS.totmat)
		nr = 0;

	if (gattribs)
		memset(gattribs, 0, sizeof(*gattribs));

	/* keep current material */
	if (nr == GMS.lastmatnr)
		return GMS.lastretval;

	/* unbind glsl material */
	if (GMS.gboundmat) {
		if (GMS.is_alpha_pass) glDepthMask(0);
		GPU_material_unbind(GPU_material_from_blender(GMS.gscene, GMS.gboundmat, GMS.is_opensubdiv));
		GMS.gboundmat = NULL;
	}

	/* draw materials with alpha in alpha pass */
	GMS.lastmatnr = nr;
	GMS.lastretval = 1;

	if (GMS.use_alpha_pass) {
		GMS.lastretval = ELEM(GMS.alphablend[nr], GPU_BLEND_SOLID, GPU_BLEND_CLIP);
		if (GMS.is_alpha_pass)
			GMS.lastretval = !GMS.lastretval;
	}
	else
		GMS.lastretval = !GMS.is_alpha_pass;

	if (GMS.lastretval) {
		/* for alpha pass, use alpha blend */
		GPUBlendMode alphablend = GMS.alphablend[nr];

		if (gattribs && GMS.gmatbuf[nr]) {
			/* bind glsl material and get attributes */
			Material *mat = GMS.gmatbuf[nr];
			GPUParticleInfo partile_info;
			float object_info[3] = {0};

			float auto_bump_scale;

			GPUMaterial *gpumat = GPU_material_from_blender(GMS.gscene, mat, GMS.is_opensubdiv);
			GPU_material_vertex_attributes(gpumat, gattribs);

			if (GMS.dob) {
				gpu_get_particle_info(&partile_info);
			}
			
			if (GPU_get_material_builtins(gpumat) & GPU_OBJECT_INFO) {
				GPU_get_object_info(object_info, mat);
			}

			GPU_material_bind(
			        gpumat, GMS.gob->lay, GMS.glay, 1.0, !(GMS.gob->mode & OB_MODE_TEXTURE_PAINT),
			        GMS.gviewmat, GMS.gviewinv, GMS.gviewcamtexcofac, GMS.gscenelock);

			auto_bump_scale = GMS.gob->derivedFinal != NULL ? GMS.gob->derivedFinal->auto_bump_scale : 1.0f;
			GPU_material_bind_uniforms(gpumat, GMS.gob->obmat, GMS.gviewmat, GMS.gob->col, auto_bump_scale, &partile_info, object_info);
			GMS.gboundmat = mat;

			/* for glsl use alpha blend mode, unless it's set to solid and
			 * we are already drawing in an alpha pass */
			if (mat->game.alpha_blend != GPU_BLEND_SOLID)
				alphablend = mat->game.alpha_blend;

			if (GMS.is_alpha_pass) glDepthMask(1);

			if (GMS.backface_culling) {
				if (mat->game.flag)
					glEnable(GL_CULL_FACE);
				else
					glDisable(GL_CULL_FACE);
			}

			if (GMS.use_matcaps)
				glColor3f(1.0f, 1.0f, 1.0f);
		}
		else {
			/* or do fixed function opengl material */
			GPU_basic_shader_colors(
			        GMS.matbuf[nr].diff,
			        GMS.matbuf[nr].spec, GMS.matbuf[nr].hard, GMS.matbuf[nr].alpha);

			if (GMS.two_sided_lighting)
				GPU_basic_shader_bind(GPU_SHADER_LIGHTING | GPU_SHADER_TWO_SIDED);
			else
				GPU_basic_shader_bind(GPU_SHADER_LIGHTING);
		}

		/* set (alpha) blending mode */
		GPU_set_material_alpha_blend(alphablend);
	}

	return GMS.lastretval;
}

int GPU_object_material_visible(int nr, void *attribs)
{
	GPUVertexAttribs *gattribs = attribs;
	int visible;

	if (!GMS.matbuf)
		return 0;

	if (gattribs)
		memset(gattribs, 0, sizeof(*gattribs));

	if (nr >= GMS.totmat)
		nr = 0;

	if (GMS.use_alpha_pass) {
		visible = ELEM(GMS.alphablend[nr], GPU_BLEND_SOLID, GPU_BLEND_CLIP);
		if (GMS.is_alpha_pass)
			visible = !visible;
	}
	else
		visible = !GMS.is_alpha_pass;

	return visible;
}

void GPU_set_material_alpha_blend(int alphablend)
{
	if (GMS.lastalphablend == alphablend)
		return;
	
	gpu_set_alpha_blend(alphablend);
	GMS.lastalphablend = alphablend;
}

int GPU_get_material_alpha_blend(void)
{
	return GMS.lastalphablend;
}

void GPU_object_material_unbind(void)
{
	GMS.lastmatnr = -1;
	GMS.lastretval = 1;

	if (GMS.gboundmat) {
		if (GMS.backface_culling)
			glDisable(GL_CULL_FACE);

		if (GMS.is_alpha_pass) glDepthMask(0);
		GPU_material_unbind(GPU_material_from_blender(GMS.gscene, GMS.gboundmat, GMS.is_opensubdiv));
		GMS.gboundmat = NULL;
	}
	else
		GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);

	GPU_set_material_alpha_blend(GPU_BLEND_SOLID);
}

void GPU_material_diffuse_get(int nr, float diff[4])
{
	/* prevent index to use un-initialized array items */
	if (nr >= GMS.totmat)
		nr = 0;

	/* no GPU_begin_object_materials, use default material */
	if (!GMS.matbuf) {
		mul_v3_v3fl(diff, &defmaterial.r, defmaterial.ref + defmaterial.emit);
	}
	else {
		copy_v3_v3(diff, GMS.matbuf[nr].diff);
		diff[3] = GMS.matbuf[nr].alpha;
	}
}

bool GPU_material_use_matcaps_get(void)
{
	return GMS.use_matcaps;
}

bool GPU_object_materials_check(void)
{
	return GMS.is_enabled;
}

void GPU_end_object_materials(void)
{
	GPU_object_material_unbind();

	GMS.is_enabled = false;

	if (GMS.matbuf && GMS.matbuf != GMS.matbuf_fixed) {
		MEM_freeN(GMS.matbuf);
		MEM_freeN(GMS.gmatbuf);
		MEM_freeN(GMS.alphablend);
	}

	GMS.matbuf = NULL;
	GMS.gmatbuf = NULL;
	GMS.alphablend = NULL;
	GMS.two_sided_lighting = false;

	/* resetting the texture matrix after the scaling needed for tiled textures */
	if (GTS.tilemode) {
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
	}
}

/* Lights */

int GPU_default_lights(void)
{
	/* initialize */
	if (U.light[0].flag == 0 && U.light[1].flag == 0 && U.light[2].flag == 0) {
		U.light[0].flag = 1;
		U.light[0].vec[0] = -0.3; U.light[0].vec[1] = 0.3; U.light[0].vec[2] = 0.9;
		U.light[0].col[0] = 0.8; U.light[0].col[1] = 0.8; U.light[0].col[2] = 0.8;
		U.light[0].spec[0] = 0.5; U.light[0].spec[1] = 0.5; U.light[0].spec[2] = 0.5;
		U.light[0].spec[3] = 1.0;
		
		U.light[1].flag = 0;
		U.light[1].vec[0] = 0.5; U.light[1].vec[1] = 0.5; U.light[1].vec[2] = 0.1;
		U.light[1].col[0] = 0.4; U.light[1].col[1] = 0.4; U.light[1].col[2] = 0.8;
		U.light[1].spec[0] = 0.3; U.light[1].spec[1] = 0.3; U.light[1].spec[2] = 0.5;
		U.light[1].spec[3] = 1.0;
	
		U.light[2].flag = 0;
		U.light[2].vec[0] = 0.3; U.light[2].vec[1] = -0.3; U.light[2].vec[2] = -0.2;
		U.light[2].col[0] = 0.8; U.light[2].col[1] = 0.5; U.light[2].col[2] = 0.4;
		U.light[2].spec[0] = 0.5; U.light[2].spec[1] = 0.4; U.light[2].spec[2] = 0.3;
		U.light[2].spec[3] = 1.0;
	}

	GPU_basic_shader_light_set_viewer(false);

	int count = 0;

	for (int a = 0; a < 8; a++) {
		if (a < 3 && U.light[a].flag) {
			GPULightData light = {0};

			light.type = GPU_LIGHT_SUN;

			normalize_v3_v3(light.direction, U.light[a].vec);
			copy_v3_v3(light.diffuse, U.light[a].col);
			copy_v3_v3(light.specular, U.light[a].spec);

			GPU_basic_shader_light_set(a, &light);

			count++;
		}
		else
			GPU_basic_shader_light_set(a, NULL);
	}

	return count;
}

int GPU_scene_object_lights(Scene *scene, Object *ob, int lay, float viewmat[4][4], int ortho)
{
	/* disable all lights */
	for (int count = 0; count < 8; count++)
		GPU_basic_shader_light_set(count, NULL);
	
	/* view direction for specular is not computed correct by default in
	 * opengl, so we set the settings ourselfs */
	GPU_basic_shader_light_set_viewer(!ortho);

	int count = 0;

	for (Base *base = scene->base.first; base; base = base->next) {
		if (base->object->type != OB_LAMP)
			continue;

		if (!(base->lay & lay) || !(base->lay & ob->lay))
			continue;

		Lamp *la = base->object->data;
		
		/* setup lamp transform */
		glPushMatrix();
		glLoadMatrixf((float *)viewmat);
		
		/* setup light */
		GPULightData light = {0};

		mul_v3_v3fl(light.diffuse, &la->r, la->energy);
		mul_v3_v3fl(light.specular, &la->r, la->energy);

		if (la->type == LA_SUN) {
			/* directional sun light */
			light.type = GPU_LIGHT_SUN;
			normalize_v3_v3(light.direction, base->object->obmat[2]);
		}
		else {
			/* other lamps with position attenuation */
			copy_v3_v3(light.position, base->object->obmat[3]);

			light.constant_attenuation = 1.0f;
			light.linear_attenuation = la->att1 / la->dist;
			light.quadratic_attenuation = la->att2 / (la->dist * la->dist);
			
			if (la->type == LA_SPOT) {
				light.type = GPU_LIGHT_SPOT;
				negate_v3_v3(light.direction, base->object->obmat[2]);
				normalize_v3(light.direction);
				light.spot_cutoff = RAD2DEGF(la->spotsize * 0.5f);
				light.spot_exponent = 128.0f * la->spotblend;
			}
			else
				light.type = GPU_LIGHT_POINT;
		}
		
		GPU_basic_shader_light_set(count, &light);
		
		glPopMatrix();
		
		count++;
		if (count == 8)
			break;
	}

	return count;
}

static void gpu_multisample(bool enable)
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
		if (enable)
			glEnable(GL_MULTISAMPLE);
		else
			glDisable(GL_MULTISAMPLE);
	}
#else
	if (enable)
		glEnable(GL_MULTISAMPLE);
	else
		glDisable(GL_MULTISAMPLE);
#endif
}

/* Default OpenGL State
 *
 * This is called on startup, for opengl offscreen render and to restore state
 * for the game engine. Generally we should always return to this state when
 * temporarily modifying the state for drawing, though that are (undocumented)
 * exceptions that we should try to get rid of. */

void GPU_state_init(void)
{
	float mat_ambient[] = { 0.0, 0.0, 0.0, 0.0 };
	float mat_specular[] = { 0.5, 0.5, 0.5, 1.0 };
	
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
	glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 35);
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);

	GPU_default_lights();
	
	glDepthFunc(GL_LEQUAL);
	/* scaling matrices */
	glEnable(GL_NORMALIZE);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LOGIC_OP);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	/* default disabled, enable should be local per function */
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
	glPixelTransferi(GL_RED_SCALE, 1);
	glPixelTransferi(GL_RED_BIAS, 0);
	glPixelTransferi(GL_GREEN_SCALE, 1);
	glPixelTransferi(GL_GREEN_BIAS, 0);
	glPixelTransferi(GL_BLUE_SCALE, 1);
	glPixelTransferi(GL_BLUE_BIAS, 0);
	glPixelTransferi(GL_ALPHA_SCALE, 1);
	glPixelTransferi(GL_ALPHA_BIAS, 0);
	
	glPixelTransferi(GL_DEPTH_BIAS, 0);
	glPixelTransferi(GL_DEPTH_SCALE, 1);
	glDepthRange(0.0, 1.0);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

	gpu_multisample(false);

	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
}

#ifdef WITH_OPENSUBDIV
/* Update face-varying variables offset which might be
 * different from mesh to mesh sharing the same material.
 */
void GPU_draw_update_fvar_offset(DerivedMesh *dm)
{
	/* Sanity check to be sure we only do this for OpenSubdiv draw. */
	BLI_assert(dm->type == DM_TYPE_CCGDM);
	BLI_assert(GMS.is_opensubdiv);

	for (int i = 0; i < GMS.totmat; ++i) {
		Material *material = GMS.gmatbuf[i];
		GPUMaterial *gpu_material;

		if (material == NULL) {
			continue;
		}

		gpu_material = GPU_material_from_blender(GMS.gscene,
		                                         material,
		                                         GMS.is_opensubdiv);

		GPU_material_update_fvar_offset(gpu_material, dm);
	}
}
#endif


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

/** \} */
