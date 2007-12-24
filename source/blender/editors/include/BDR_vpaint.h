/**
 * $Id: BDR_vpaint.h 8971 2006-11-20 12:08:06Z ton $
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

#ifndef BDR_VPAINT_H
#define BDR_VPAINT_H

struct Mesh;
struct MDeformVert;	/* __NLA */

unsigned int rgba_to_mcol(float r, float g, float b, float a);
void do_shared_vertexcol(struct Mesh *me);
void make_vertexcol(int shade);

void clear_vpaint(void);
void clear_vpaint_selectedfaces(void);
void vpaint_dogamma(void);
void sample_vpaint(void);

void free_vertexpaint(void);
void vertex_paint(void);
void set_vpaint(void); 

void set_wpaint(void);
void clear_wpaint_selectedfaces(void);
void weight_paint(void);


#endif /*  BDR_VPAINT_H */

