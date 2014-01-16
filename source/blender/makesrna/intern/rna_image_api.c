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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Arystanbek Dyussenov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_image_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "DNA_packedFile_types.h"

#include "BLI_utildefines.h"

#include "BIF_gl.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include "BKE_image.h"
#include "BKE_packedFile.h"
#include "BKE_main.h"

#include "BKE_global.h" /* grr: G.main->name */

#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "BIF_gl.h"
#include "GPU_draw.h"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

static void rna_Image_save_render(Image *image, bContext *C, ReportList *reports, const char *path, Scene *scene)
{
	ImBuf *ibuf;

	if (scene == NULL) {
		scene = CTX_data_scene(C);
	}

	if (scene) {
		ImageUser iuser;
		void *lock;

		iuser.scene = scene;
		iuser.ok = 1;

		ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

		if (ibuf == NULL) {
			BKE_report(reports, RPT_ERROR, "Could not acquire buffer from image");
		}
		else {
			ImBuf *write_ibuf;

			write_ibuf = IMB_colormanagement_imbuf_for_write(ibuf, true, true, &scene->view_settings,
			                                                 &scene->display_settings, &scene->r.im_format);

			write_ibuf->planes = scene->r.im_format.planes;
			write_ibuf->dither = scene->r.dither_intensity;

			if (!BKE_imbuf_write(write_ibuf, path, &scene->r.im_format)) {
				BKE_reportf(reports, RPT_ERROR, "Could not write image '%s'", path);
			}

			if (write_ibuf != ibuf)
				IMB_freeImBuf(write_ibuf);
		}

		BKE_image_release_ibuf(image, ibuf, lock);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Scene not in context, could not get save parameters");
	}
}

static void rna_Image_save(Image *image, ReportList *reports)
{
	ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);
	if (ibuf) {
		char filename[FILE_MAX];
		BLI_strncpy(filename, image->name, sizeof(filename));
		BLI_path_abs(filename, G.main->name);

		if (image->packedfile) {
			if (writePackedFile(reports, image->name, image->packedfile, 0) != RET_OK) {
				BKE_reportf(reports, RPT_ERROR, "Image '%s' could not save packed file to '%s'", image->id.name + 2, image->name);
			}
		}
		else if (IMB_saveiff(ibuf, filename, ibuf->flags)) {
			image->type = IMA_TYPE_IMAGE;

			if (image->source == IMA_SRC_GENERATED)
				image->source = IMA_SRC_FILE;

			IMB_colormanagment_colorspace_from_ibuf_ftype(&image->colorspace_settings, ibuf);

			ibuf->userflags &= ~IB_BITMAPDIRTY;
		}
		else {
			BKE_reportf(reports, RPT_ERROR, "Image '%s' could not be saved to '%s'", image->id.name + 2, image->name);
		}
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
	}

	BKE_image_release_ibuf(image, ibuf, NULL);
}

static void rna_Image_pack(Image *image, ReportList *reports, int as_png)
{
	ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);

	if (!as_png && (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))) {
		BKE_report(reports, RPT_ERROR, "Cannot pack edited image from disk, only as internal PNG");
	}
	else {
		if (as_png) {
			BKE_image_memorypack(image);
		}
		else {
			image->packedfile = newPackedFile(reports, image->name, ID_BLEND_PATH(G.main, &image->id));
		}
	}

	BKE_image_release_ibuf(image, ibuf, NULL);
}

static void rna_Image_unpack(Image *image, ReportList *reports, int method)
{
	if (!image->packedfile) {
		BKE_report(reports, RPT_ERROR, "Image not packed");
	}
	else if (BKE_image_is_animated(image)) {
		BKE_report(reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
		return;
	}
	else {
		/* reports its own error on failure */
		unpackImage(reports, image, method);
	}
}

static void rna_Image_reload(Image *image)
{
	BKE_image_signal(image, NULL, IMA_SIGNAL_RELOAD);
}

static void rna_Image_update(Image *image, ReportList *reports)
{
	ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);

	if (ibuf == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
		return;
	}

	if (ibuf->rect)
		IMB_rect_from_float(ibuf);

	ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

	BKE_image_release_ibuf(image, ibuf, NULL);
}

static void rna_Image_scale(Image *image, ReportList *reports, int width, int height)
{
	if (!BKE_image_scale(image, width, height)) {
		BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
	}
}

static int rna_Image_gl_load(Image *image, ReportList *reports, int frame, int filter, int mag)
{
	ImBuf *ibuf;
	unsigned int *bind = &image->bindcode;
	int error = GL_NO_ERROR;
	ImageUser iuser = {NULL};
	void *lock;

	if (*bind)
		return error;
	iuser.framenr = frame;
	iuser.ok = true;

	ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

	if (ibuf == NULL || ibuf->rect == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
		BKE_image_release_ibuf(image, ibuf, NULL);
		return (int)GL_INVALID_OPERATION;
	}

	/* could be made into a function? */
	glGenTextures(1, (GLuint *)bind);
	glBindTexture(GL_TEXTURE_2D, *bind);

	if (filter != GL_NEAREST && filter != GL_LINEAR)
		error = (int)gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

	if (!error) {
		/* clean glError buffer */
		while (glGetError() != GL_NO_ERROR) {}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, image->tpageflag & IMA_CLAMP_U ? GL_CLAMP : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, image->tpageflag & IMA_CLAMP_V ? GL_CLAMP : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)mag);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ibuf->x, ibuf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
		error = (int)glGetError();
	}

	if (error) {
		glDeleteTextures(1, (GLuint *)bind);
		image->bindcode = 0;
	}

	BKE_image_release_ibuf(image, ibuf, NULL);

	return error;
}

static int rna_Image_gl_touch(Image *image, ReportList *reports, int frame, int filter, int mag)
{
	unsigned int *bind = &image->bindcode;
	int error = GL_NO_ERROR;

	BKE_image_tag_time(image);

	if (*bind == 0)
		error = rna_Image_gl_load(image, reports, frame, filter, mag);

	return error;
}

static void rna_Image_gl_free(Image *image)
{
	GPU_free_image(image);

	/* remove the nocollect flag, image is available for garbage collection again */
	image->flag &= ~IMA_NOCOLLECT;
}

#else

void RNA_api_image(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "save_render", "rna_Image_save_render");
	RNA_def_function_ui_description(func, "Save image to a specific path using a scenes render settings");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_string_file_path(func, "filepath", NULL, 0, "", "Save path");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_pointer(func, "scene", "Scene", "", "Scene to take image parameters from");

	func = RNA_def_function(srna, "save", "rna_Image_save");
	RNA_def_function_ui_description(func, "Save image to its source path");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	func = RNA_def_function(srna, "pack", "rna_Image_pack");
	RNA_def_function_ui_description(func, "Pack an image as embedded data into the .blend file");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_boolean(func, "as_png", 0, "as_png", "Pack the image as PNG (needed for generated/dirty images)");

	func = RNA_def_function(srna, "unpack", "rna_Image_unpack");
	RNA_def_function_ui_description(func, "Save an image packed in the .blend file to disk");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_enum(func, "method", unpack_method_items, PF_USE_LOCAL, "method", "How to unpack");

	func = RNA_def_function(srna, "reload", "rna_Image_reload");
	RNA_def_function_ui_description(func, "Reload the image from its source path");

	func = RNA_def_function(srna, "update", "rna_Image_update");
	RNA_def_function_ui_description(func, "Update the display image from the floating point buffer");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	func = RNA_def_function(srna, "scale", "rna_Image_scale");
	RNA_def_function_ui_description(func, "Scale the image in pixels");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_int(func, "width", 1, 1, 10000, "", "Width", 1, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "height", 1, 1, 10000, "", "Height", 1, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "gl_touch", "rna_Image_gl_touch");
	RNA_def_function_ui_description(func, "Delay the image from being cleaned from the cache due inactivity");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "frame", 0, 0, INT_MAX, "Frame",
	            "Frame of image sequence or movie", 0, INT_MAX);
	RNA_def_int(func, "filter", GL_LINEAR_MIPMAP_NEAREST, -INT_MAX, INT_MAX, "Filter",
	            "The texture minifying function to use if the image wasn't loaded", -INT_MAX, INT_MAX);
	RNA_def_int(func, "mag", GL_LINEAR, -INT_MAX, INT_MAX, "Magnification",
	            "The texture magnification function to use if the image wasn't loaded", -INT_MAX, INT_MAX);
	/* return value */
	parm = RNA_def_int(func, "error", 0, -INT_MAX, INT_MAX, "Error", "OpenGL error value", -INT_MAX, INT_MAX);
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "gl_load", "rna_Image_gl_load");
	RNA_def_function_ui_description(func, "Load the image into OpenGL graphics memory");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "frame", 0, 0, INT_MAX, "Frame",
	            "Frame of image sequence or movie", 0, INT_MAX);
	RNA_def_int(func, "filter", GL_LINEAR_MIPMAP_NEAREST, -INT_MAX, INT_MAX, "Filter",
	            "The texture minifying function", -INT_MAX, INT_MAX);
	RNA_def_int(func, "mag", GL_LINEAR, -INT_MAX, INT_MAX, "Magnification",
	            "The texture magnification function", -INT_MAX, INT_MAX);

	/* return value */
	parm = RNA_def_int(func, "error", 0, -INT_MAX, INT_MAX, "Error", "OpenGL error value", -INT_MAX, INT_MAX);
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "gl_free", "rna_Image_gl_free");
	RNA_def_function_ui_description(func, "Free the image from OpenGL graphics memory");

	/* TODO, pack/unpack, maybe should be generic functions? */
}

#endif

