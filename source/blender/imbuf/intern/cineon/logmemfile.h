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
 *	 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
 
#ifndef _LOGMEMFILE_H
#define _LOGMEMFILE_H

int logimage_fseek(void* logfile, intptr_t offsett, int origin);
int logimage_fwrite(void *buffer, unsigned int size, unsigned int count, void *logfile);
int logimage_fread(void *buffer, unsigned int size, unsigned int count, void *logfile);

#endif /* _LOGMEMFILE_H */
