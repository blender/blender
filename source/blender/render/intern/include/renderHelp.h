/*
 * renderhelp_ext.h
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

#ifndef RENDERHELP_EXT_H
#define RENDERHELP_EXT_H 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __cplusplus
extern "C" { 
#endif

	/* Push-pop, because this sometimes is necessary... */
	void pushTempPanoPhi(float p);
	void popTempPanoPhi(void);
	
	float getPanoPhi(void);
	float getPanovCo(void);
	float getPanovSi(void);
	void setPanoRot(int part);

	/** Set clip flags on all data entries, using the given projection
	 * function */
	void setzbufvlaggen( void (*projectfunc)(float *, float *) );

/* external for the time being, since the converter calls it. */
/** Recalculate all normals on renderdata. */
/*  	void set_normalflags(void); */

#ifdef __cplusplus
}
#endif

#endif

