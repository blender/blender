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
 * streamglue loopback. Needed at start of Read stream.
 */

#include <stdlib.h> // TODO use blender's

#include "BLO_readStreamGlue.h"
#include "BLO_readStreamGlueLoopBack.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

	struct readStreamGlueLoopBackStruct *
readStreamGlueLoopBack_begin(
	void *endControl)
{
	struct readStreamGlueLoopBackStruct *control;
	control = malloc(sizeof(struct readStreamGlueLoopBackStruct));
	if (control == NULL) {
		return NULL;
	}

	control->streamGlue = NULL;
	control->endControl = endControl;

	return(control);
}

	int
readStreamGlueLoopBack_process(
	struct readStreamGlueLoopBackStruct *control,
	unsigned char *data,
	unsigned int dataIn)
{
	int err = 0;
	/* Is there really new data available ? */
	if (dataIn > 0) {
		err = readStreamGlue(
			control->endControl,
			&(control->streamGlue),
			data,
			dataIn);
	}
	return err;
}

	int
readStreamGlueLoopBack_end(
	struct readStreamGlueLoopBackStruct *control)
{
	free(control);
	return 0;
}

