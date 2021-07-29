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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/intern/blf_font_win32_compat.c
 *  \ingroup blf
 *
 * Workaround for win32 which needs to use BLI_fopen to access files.
 *
 * defines #FT_New_Face__win32_compat, a drop-in replacement for \a #FT_New_Face.
 */

#ifdef WIN32

#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_fileops.h"

#include "blf_internal.h"

/* internal freetype defines */
#define STREAM_FILE(stream)  ((FILE *)stream->descriptor.pointer)
#define FT_THROW(e) -1

static void ft_ansi_stream_close(
        FT_Stream stream)
{
	fclose(STREAM_FILE(stream));

	stream->descriptor.pointer = NULL;
	stream->size               = 0;
	stream->base               = 0;

	/* WARNING: this works but be careful!
	 * Checked freetype sources, there isn't any access after closing. */
	MEM_freeN(stream);
}

static unsigned long ft_ansi_stream_io(
        FT_Stream       stream,
        unsigned long   offset,
        unsigned char  *buffer,
        unsigned long   count)
{
	FILE *file;
	if (!count && offset > stream->size)
		return 1;

	file = STREAM_FILE(stream);

	if (stream->pos != offset)
		fseek(file, offset, SEEK_SET);

	return fread(buffer, 1, count, file);
}

static FT_Error FT_Stream_Open__win32_compat(FT_Stream stream, const char *filepathname)
{
	FILE *file;
	BLI_assert(stream);

	stream->descriptor.pointer = NULL;
	stream->pathname.pointer   = (char *)filepathname;
	stream->base               = 0;
	stream->pos                = 0;
	stream->read               = NULL;
	stream->close              = NULL;

	file = BLI_fopen(filepathname, "rb");
	if (!file) {
		fprintf(stderr,
		        "FT_Stream_Open: "
		        "could not open `%s'\n", filepathname);
		return FT_THROW(Cannot_Open_Resource);
	}

	fseek(file, 0, SEEK_END);
	stream->size = ftell(file);
	if (!stream->size) {
		fprintf(stderr,
		        "FT_Stream_Open: "
		        "opened `%s' but zero-sized\n", filepathname);
		fclose(file);
		return FT_THROW(Cannot_Open_Stream);
	}

	fseek(file, 0, SEEK_SET);

	stream->descriptor.pointer = file;
	stream->read  = ft_ansi_stream_io;
	stream->close = ft_ansi_stream_close;

	return FT_Err_Ok;
}

FT_Error FT_New_Face__win32_compat(
        FT_Library   library,
        const char  *pathname,
        FT_Long      face_index,
        FT_Face     *aface)
{
	FT_Error err;
	FT_Open_Args open;
	FT_Stream stream = NULL;
	stream = MEM_callocN(sizeof(*stream), __func__);

	open.flags = FT_OPEN_STREAM;
	open.stream = stream;
	stream->pathname.pointer = (char *)pathname;

	err = FT_Stream_Open__win32_compat(stream, pathname);
	if (err) {
		MEM_freeN(stream);
		return err;
	}

	err = FT_Open_Face(library, &open, face_index, aface);
	/* no need to free 'stream', its handled by FT_Open_Face if an error occurs */

	return err;
}

#endif  /* WIN32 */
