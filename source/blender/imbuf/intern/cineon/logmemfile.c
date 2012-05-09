/*
 *	 Cineon image file format library routines.
 *
 *	 Copyright 2006 Joseph Eagar (joeedh@gmail.com)
 *
 *	 This program is free software; you can redistribute it and/or modify it
 *	 under the terms of the GNU General Public License as published by the Free
 *	 Software Foundation; either version 2 of the License, or (at your option)
 *	 any later version.
 *
 *	 This program is distributed in the hope that it will be useful, but
 *	 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *	 or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU General Public License
 *	 for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the Free Software
 *	 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/** \file blender/imbuf/intern/cineon/logmemfile.c
 *  \ingroup imbcineon
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logImageCore.h"

#include "logmemfile.h" /* own include */

int logimage_fseek(void* logfile, intptr_t offsett, int origin)
{	
	struct _Log_Image_File_t_ *file = (struct _Log_Image_File_t_*) logfile;
	intptr_t offset = offsett;
	
	if (file->file) fseek(file->file, offset, origin);
	else { /*we're seeking in memory*/
		if (origin==SEEK_SET) {
			if (offset > file->membuffersize) return 1;
			file->memcursor = file->membuffer + offset;
		}
		else if (origin==SEEK_END) {
			if (offset > file->membuffersize) return 1;
			file->memcursor = (file->membuffer + file->membuffersize) - offset;
		}
		else if (origin==SEEK_CUR) {
			uintptr_t pos = (uintptr_t)file->membuffer - (uintptr_t)file->memcursor;
			if (pos + offset > file->membuffersize) return 1;
			if (pos < 0) return 1;
			file->memcursor += offset;
		}
	}
	return 0;
}

int logimage_fwrite(void *buffer, unsigned int size, unsigned int count, void *logfile)
{
	struct _Log_Image_File_t_ *file = (struct _Log_Image_File_t_*) logfile;
	if (file->file) return fwrite(buffer, size, count, file->file);
	else { /*we're writing to memory*/
		/*do nothing as this isn't supported yet*/
		return count;
	}
}

int logimage_fread(void *buffer, unsigned int size, unsigned int count, void *logfile)
{
	struct _Log_Image_File_t_ *file = (struct _Log_Image_File_t_*) logfile;
	if (file->file) return fread(buffer, size, count, file->file);
	else { /*we're reading from memory*/
		int i;
		/* we convert ot uchar just on the off chance some platform can't handle
		 * pointer arithmetic with type (void*). */
		unsigned char *buf = (unsigned char *) buffer; 
		
		for (i=0; i<count; i++) {
			memcpy(buf, file->memcursor, size);
			file->memcursor += size;
			buf += size;
		}
		return count;
	}
}
