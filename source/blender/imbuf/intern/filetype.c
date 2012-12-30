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
 * Contributor(s): Blender Foundation, 2010.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/filetype.c
 *  \ingroup imbuf
 */


#include <stddef.h>
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_filetype.h"

#include "IMB_colormanagement.h"

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

static int imb_ftype_default(ImFileType *type, ImBuf *ibuf)
{
	return (ibuf->ftype & type->filetype);
}
static int imb_ftype_iris(ImFileType *type, ImBuf *ibuf)
{
	(void)type;
	return (ibuf->ftype == IMAGIC);
}
#ifdef WITH_QUICKTIME
static int imb_ftype_quicktime(ImFileType *type, ImBuf *ibuf)
{
	return 0; /* XXX */
}
#endif

#ifdef WITH_QUICKTIME
void quicktime_init(void);
void quicktime_exit(void);
#endif

ImFileType IMB_FILE_TYPES[] = {
	{NULL, NULL, imb_is_a_jpeg, imb_ftype_default, imb_load_jpeg, imb_savejpeg, NULL, 0, JPG, COLOR_ROLE_DEFAULT_BYTE},
	{NULL, NULL, imb_is_a_png, imb_ftype_default, imb_loadpng, imb_savepng, NULL, IM_FTYPE_FLOAT, PNG, COLOR_ROLE_DEFAULT_BYTE},
	{NULL, NULL, imb_is_a_bmp, imb_ftype_default, imb_bmp_decode, imb_savebmp, NULL, 0, BMP, COLOR_ROLE_DEFAULT_BYTE},
	{NULL, NULL, imb_is_a_targa, imb_ftype_default, imb_loadtarga, imb_savetarga, NULL, 0, TGA, COLOR_ROLE_DEFAULT_BYTE},
	{NULL, NULL, imb_is_a_iris, imb_ftype_iris, imb_loadiris, imb_saveiris, NULL, 0, IMAGIC, COLOR_ROLE_DEFAULT_BYTE},
#ifdef WITH_CINEON
	{NULL, NULL, imb_is_dpx, imb_ftype_default, imb_load_dpx, imb_save_dpx, NULL, IM_FTYPE_FLOAT, DPX, COLOR_ROLE_DEFAULT_FLOAT},
	{NULL, NULL, imb_is_cineon, imb_ftype_default, imb_load_cineon, imb_save_cineon, NULL, IM_FTYPE_FLOAT, CINEON, COLOR_ROLE_DEFAULT_FLOAT},
#endif
#ifdef WITH_TIFF
	{imb_inittiff, NULL, imb_is_a_tiff, imb_ftype_default, imb_loadtiff, imb_savetiff, imb_loadtiletiff, 0, TIF, COLOR_ROLE_DEFAULT_BYTE},
#endif
#ifdef WITH_HDR
	{NULL, NULL, imb_is_a_hdr, imb_ftype_default, imb_loadhdr, imb_savehdr, NULL, IM_FTYPE_FLOAT, RADHDR, COLOR_ROLE_DEFAULT_FLOAT},
#endif
#ifdef WITH_OPENEXR
	{imb_initopenexr, NULL, imb_is_a_openexr, imb_ftype_default, imb_load_openexr, imb_save_openexr, NULL, IM_FTYPE_FLOAT, OPENEXR, COLOR_ROLE_DEFAULT_FLOAT},
#endif
#ifdef WITH_OPENJPEG
	{NULL, NULL, imb_is_a_jp2, imb_ftype_default, imb_jp2_decode, imb_savejp2, NULL, IM_FTYPE_FLOAT, JP2, COLOR_ROLE_DEFAULT_BYTE},
#endif
#ifdef WITH_DDS
	{NULL, NULL, imb_is_a_dds, imb_ftype_default, imb_load_dds, NULL, NULL, 0, DDS, COLOR_ROLE_DEFAULT_BYTE},
#endif
#ifdef WITH_QUICKTIME
	{quicktime_init, quicktime_exit, imb_is_a_quicktime, imb_ftype_quicktime, imb_quicktime_decode, NULL, NULL, 0, QUICKTIME, COLOR_ROLE_DEFAULT_BYTE},
#endif
	{NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0}
};
	
void imb_filetypes_init(void)
{
	ImFileType *type;

	for (type = IMB_FILE_TYPES; type->is_a; type++)
		if (type->init)
			type->init();
}

void imb_filetypes_exit(void)
{
	ImFileType *type;

	for (type = IMB_FILE_TYPES; type->is_a; type++)
		if (type->exit)
			type->exit();
}

