/*
 * errorHandler.h
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H 

/* ------------------------------------------------------------------------- */
/* error codes */
enum RE_RENDER_ERROR {
	RE_NO_ERROR,
	RE_DEPTH_MISMATCH,     /* 1.  conflict resolution detects a bad z value  */
	RE_BAD_FACE_TYPE,      /* 2. a face type switch fails                    */
	RE_BAD_FACE_INDEX,     /* 3. tried to do an operation with a bad index   */
	RE_BAD_DATA_POINTER,
	RE_TRACE_COUNTER,
	RE_TOO_MANY_FACES,     /* 6. overflow on z-buffer depth                  */
	RE_EDGERENDER_WRITE_OUTSIDE_BUFFER, /* 7. write value outside buffer     */
	RE_CANNOT_ALLOCATE_MEMORY, /* 8. no memory for malloc                    */
	RE_WRITE_OUTSIDE_COLOUR_BUFFER, /* 9. write outside colour target buffer */
	RE_MAX_ERROR
};

/**
 * Reset all counters for the error trace
 */
void RE_errortrace_reset(void);

/**
 * Signals an error to screen. Counts repetitive errors
 */
void RE_error(int errType, char* fname);

/**
 * Signals an error, and prints an integer argument
 */
void RE_error_int(int errType, char* fname, int valye);

#endif /* ERRORHANDLER_H */

