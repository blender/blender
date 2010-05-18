/*
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
 * Contributor(s): Blender Foundation, 2010.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_filetype.h"

#ifdef WITH_OPENEXR
#include "openexr/openexr_api.h"
#endif

#ifdef WITH_DDS
#include "dds/dds_api.h"
#endif

#ifdef WITH_QUICKTIME
#include "quicktime_import.h"
#endif

#include "imbuf.h"

static int imb_ftype_default(ImFileType *type, ImBuf *ibuf) { return (ibuf->ftype & type->filetype); }
#if defined(__APPLE__) && defined(IMBUF_COCOA)
static int imb_ftype_cocoa(ImFileType *type, ImBuf *ibuf) { return (ibuf->ftype & TIF); }
#endif
static int imb_ftype_iris(ImFileType *type, ImBuf *ibuf) { return (ibuf->ftype == IMAGIC); }
#ifdef WITH_QUICKTIME
static int imb_ftype_quicktime(ImFileType *type, ImBuf *ibuf) { return 0; } // XXX
#endif

#ifdef WITH_QUICKTIME
void quicktime_init(void);
void quicktime_exit(void);
#endif

void libtiff_init(void);
void libtiff_exit(void);

ImFileType IMB_FILE_TYPES[]= {
	{NULL, NULL, imb_is_a_iris, imb_ftype_iris, imb_loadiris, imb_saveiris, NULL, 0, IMAGIC},
	{NULL, NULL, imb_is_a_jpeg, imb_ftype_default, imb_load_jpeg, imb_savejpeg, NULL, 0, JPG},
	{NULL, NULL, imb_is_a_png, imb_ftype_default, imb_loadpng, imb_savepng, NULL, 0, PNG},
	{NULL, NULL, imb_is_a_bmp, imb_ftype_default, imb_bmp_decode, imb_savebmp, NULL, 0, BMP},
	{NULL, NULL, imb_is_a_targa, imb_ftype_default, imb_loadtarga, imb_savetarga, NULL, 0, TGA},
	{NULL, NULL, imb_is_dpx, imb_ftype_default, imb_loaddpx, imb_save_dpx, NULL, IM_FTYPE_FLOAT, DPX},
	{NULL, NULL, imb_is_cineon, imb_ftype_default, imb_loadcineon, imb_savecineon, NULL, IM_FTYPE_FLOAT, CINEON},
#if defined(__APPLE__) && defined(IMBUF_COCOA)
	{NULL, NULL, imb_is_a_cocoa, imb_ftype_cocoa, imb_imb_cocoaLoadImage, imb_savecocoa, NULL, 0, TIF},
#else
	{libtiff_init, libtiff_exit, imb_is_a_tiff, imb_ftype_default, imb_loadtiff, imb_savetiff, imb_loadtiletiff, 0, TIF},
#endif
	{NULL, NULL, imb_is_a_hdr, imb_ftype_default, imb_loadhdr, imb_savehdr, NULL, IM_FTYPE_FLOAT, RADHDR},
#ifdef WITH_OPENEXR
	{NULL, NULL, imb_is_a_openexr, imb_ftype_default, imb_load_openexr, imb_save_openexr, NULL, IM_FTYPE_FLOAT, OPENEXR},
#endif
#ifdef WITH_OPENJPEG
	{NULL, NULL, imb_is_a_jp2, imb_ftype_default, imb_jp2_decode, imb_savejp2, NULL, IM_FTYPE_FLOAT, JP2},
#endif
#ifdef WITH_DDS
	{NULL, NULL, imb_is_a_dds, imb_ftype_default, imb_load_dds, NULL, NULL, 0, DDS},
#endif
#ifdef WITH_QUICKTIME
	{quicktime_init, quicktime_exit, imb_is_a_quicktime, imb_ftype_quicktime, imb_quicktime_decode, NULL, NULL, 0, QUICKTIME},
#endif	
	{NULL, NULL, NULL, NULL, NULL, NULL, 0}};
	
void imb_filetypes_init(void)
{
	ImFileType *type;

	for(type=IMB_FILE_TYPES; type->is_a; type++)
		if(type->init)
			type->init();
}

void imb_filetypes_exit(void)
{
	ImFileType *type;

	for(type=IMB_FILE_TYPES; type->is_a; type++)
		if(type->exit)
			type->exit();
}

