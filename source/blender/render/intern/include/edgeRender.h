/*
 * edgeRender.h
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

#ifndef EDGERENDER_H
#define EDGERENDER_H 

/**
 * Add edges to <targetbuf>, which is of size <iw> by <ih>. Use <osanr>
 * samples, and intensity <i>. <compat> indicates an extra shift in the
 * image, for backwards compatibility with the old renderpipe. <mode>
 * indicates which edges should be considered. The edges will be shaded
 * to <rgb>
 */
void
addEdges(
	char * targetbuf,
	int iw,
	int ih,
	int osanr,
	short int i,
	short int i_red,
	int compat,
	int mode,
	float r,
	float g,
	float b
	);

/* ------------------------------------------------------------------------- */

#endif /* EDGERENDER_H */

