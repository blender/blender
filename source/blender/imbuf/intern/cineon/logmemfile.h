/*
 * Cineon image file format library routines.
 *
 * Copyright 2006 Joseph Eagar (joeedh@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Julien Enche.
 *
 */

/** \file blender/imbuf/intern/cineon/logmemfile.h
 *  \ingroup imbcineon
 */


#ifndef __LOGMEMFILE_H__
#define __LOGMEMFILE_H__

#include "logImageCore.h"

#include <stdlib.h>

int logimage_fseek(LogImageFile *logFile, intptr_t offset, int origin);
int logimage_fwrite(void *buffer, size_t size, unsigned int count, LogImageFile *logFile);
int logimage_fread(void *buffer, size_t size, unsigned int count, LogImageFile *logFile);
int logimage_read_uchar(unsigned char *x, LogImageFile *logFile);
int logimage_read_ushort(unsigned short *x, LogImageFile *logFile);
int logimage_read_uint(unsigned int *x, LogImageFile *logFile);

#endif  /* __LOGMEMFILE_H__ */
