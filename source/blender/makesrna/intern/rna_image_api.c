/**
 * $Id$
 * 
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"

#ifdef RNA_RUNTIME

#include "BKE_image.h"
#include "BKE_packedFile.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BKE_global.h" /* grr: G.sce */

#include "IMB_imbuf.h"

#include "BIF_gl.h"
#include "GPU_draw.h"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

static void rna_Image_save_render(Image *image, bContext *C, ReportList *reports, char *path, Scene *scene)
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
			BKE_reportf(reports, RPT_ERROR, "Couldn't acquire buffer from image");
		}

		if (!BKE_write_ibuf(NULL, ibuf, path, scene->r.imtype, scene->r.subimtype, scene->r.quality)) {
			BKE_reportf(reports, RPT_ERROR, "Couldn't write image: %s", path);
		}
	} else {
		BKE_reportf(reports, RPT_ERROR, "Scene not in context, couldn't get save parameters");
	}
}

static void rna_Image_save(Image *image, ReportList *reports)
{
	ImBuf *ibuf= BKE_image_get_ibuf(image, NULL);
	if(ibuf) {
		char filename[FILE_MAXDIR + FILE_MAXFILE];
		BLI_strncpy(filename, image->name, sizeof(filename));
		BLI_path_abs(filename, G.sce);

		if(image->packedfile) {
			if (writePackedFile(reports, image->name, image->packedfile, 0) != RET_OK) {
				BKE_reportf(reports, RPT_ERROR, "Image \"%s\" could saved packed file to \"%s\"", image->id.name+2, image->name);
			}
		}
		else if (IMB_saveiff(ibuf, filename, ibuf->flags)) {
			image->type= IMA_TYPE_IMAGE;

			if(image->source==IMA_SRC_GENERATED)
				image->source= IMA_SRC_FILE;

			ibuf->userflags &= ~IB_BITMAPDIRTY;
		}
		else {
			BKE_reportf(reports, RPT_ERROR, "Image \"%s\" could not be saved to \"%s\"", image->id.name+2, image->name);
		}
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "Image \"%s\" does not have any image data", image->id.name+2);
	}
}

static void rna_Image_reload(Image *image)
{
	BKE_image_signal(image, NULL, IMA_SIGNAL_RELOAD);
}

static void rna_Image_update(Image *image, ReportList *reports)
{
	ImBuf *ibuf= BKE_image_get_ibuf(image, NULL);

	if(ibuf == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Image \"%s\" does not have any image data", image->id.name+2);
		return;
	}

	IMB_rect_from_float(ibuf);
}

static void rna_Image_gl_load(Image *image, ReportList *reports)
{
	ImBuf *ibuf;
	unsigned int *bind = &image->bindcode;

	if(*bind)
		return;

	ibuf= BKE_image_get_ibuf(image, NULL);

	if(ibuf == NULL || ibuf->rect == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Image \"%s\" does not have any image data", image->id.name+2);
		return;
	}

	/* could be made into a function? */
	glGenTextures( 1, ( GLuint * ) bind );
	glBindTexture( GL_TEXTURE_2D, *bind );

	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ibuf->x, ibuf->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
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

	func= RNA_def_function(srna, "save_render", "rna_Image_save_render");
	RNA_def_function_ui_description(func, "Save image to a specific path using a scenes render settings");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_string(func, "path", "", 0, "", "Save path.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "scene", "Scene", "", "Scene to take image parameters from");

	func= RNA_def_function(srna, "save", "rna_Image_save");
	RNA_def_function_ui_description(func, "Save image to its source path");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	func= RNA_def_function(srna, "reload", "rna_Image_reload");
	RNA_def_function_ui_description(func, "Reload the image from its source path");

	func= RNA_def_function(srna, "update", "rna_Image_update");
	RNA_def_function_ui_description(func, "Update the display image from the floating point buffer");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	func= RNA_def_function(srna, "gl_load", "rna_Image_gl_load");
	RNA_def_function_ui_description(func, "Load the image into OpenGL graphics memory");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	func= RNA_def_function(srna, "gl_free", "rna_Image_gl_free");
	RNA_def_function_ui_description(func, "Free the image from OpenGL graphics memory");

	/* TODO, pack/unpack, maybe should be generic functions? */
}

#endif

