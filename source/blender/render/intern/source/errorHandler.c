/**
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
 * Error handler for the rendering code. Maybe also useful elsewhere?
 */

#include "GEN_messaging.h"
#include "stdio.h"
#include "errorHandler.h"
#include "render_intern.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */

/* counters for error handling */
static int lastError;         /* code of last encountered error              */
static int errorCount;        /* count how many time it occured              */
/* ------------------------------------------------------------------------- */

char errorStrings[RE_MAX_ERROR][100] = {
    "0: No error",
    "1: recalculated depth falls outside original range",
    "2: invalid face/halo type",
    "3: invalid face index",
    "4: invalid data pointer",
	"5: generic trace counter",
	"6: overflow on z buffer depth",
	"7: write outside edgerender buffer",
	"8: cannot allocate memory",
	"9: write outside colour target buffer",
};

/* ------------------------------------------------------------------------- */

void RE_errortrace_reset(void)
{
	lastError = RE_NO_ERROR;
	errorCount = 0;
}

void RE_error(int errType, char* fname)
{
	/*
	 * This memory behaviour should move to the generic stream...
	 */
	
    if (lastError == errType) {
        int teller;
        errorCount++;
        for (teller = 0; teller < 12; teller++)
            fprintf(GEN_errorstream, "%c", 0x08); /* backspaces */
        fprintf(GEN_errorstream, "( %8u )", errorCount);
    } else {
        fprintf(GEN_errorstream, "\n*** %s: %s             ", 
                fname, errorStrings[errType]);
        lastError = errType;
        errorCount = 1;
    }    
} /* end of void RE_error(int errType, char* errText) */

/* ------------------------------------------------------------------------- */
/* note: non-repeating */
void RE_error_int(int errType, char* fname, int value)
{
	fprintf(GEN_errorstream, "\n*** %s: %s : %d", 
			fname, errorStrings[errType], value);
	lastError = RE_NO_ERROR;
} /* end of void RE_error_int(int errType, char* errText, int value) */

/* ------------------------------------------------------------------------- */

/* eof */
