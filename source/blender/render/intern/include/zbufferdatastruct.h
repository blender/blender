/*
 * zbufferdatastruct_ext.h
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

#ifndef ZBUFFERDATASTRUCT_EXT_H
#define ZBUFFERDATASTRUCT_EXT_H

#include "zbufferdatastruct_types.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * Set memory and counters for a fresh z buffer
 */
void initZbuffer(int linewidth);

/**
 * Release memory for the current z buffer
 */
void freeZbuffer(void);

/**
 * Release previous buffer and initialise new buffer. 
 */
void resetZbuffer(void);

/**
 * Make a root for a memory block (internal)
 */
RE_APixstrExt  *addpsemainA(void);

/**
 * Release a memory chunk
 */
void            freepseA(void);

/**
 * Add a structure
 */
RE_APixstrExt  *addpseA(void);

/**
 * Add an object to a zbuffer entry.
 */
void insertObject(int teller,
/*  				  int opaque, */
				  int obindex,
				  int obtype, 
				  int dist, 
				  int mask);

/**
 * Add a flat object to a zbuffer entry.
 */
void insertFlatObject(RE_APixstrExt* ap, 
					  int obindex,
					  int obtype, 
					  int dist, 
					  int mask);

/**
 * Add a flat object to a zbuffer entry, but don't do OSA entry testing.
 */
void insertFlatObjectNoOsa(RE_APixstrExt* ap, 
						   int obindex,
						   int obtype, 
						   int dist, 
						   int mask);

#endif

