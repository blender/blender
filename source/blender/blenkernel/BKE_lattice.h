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
 * june-2001 ton
 */

#ifndef BKE_LATTICE_H
#define BKE_LATTICE_H

struct Lattice;
struct Object;
struct BPoint;

extern struct Lattice *editLatt;


void resizelattice(struct Lattice *lt);
struct Lattice *add_lattice(void);
struct Lattice *copy_lattice(struct Lattice *lt);
void free_lattice(struct Lattice *lt);
void make_local_lattice(struct Lattice *lt);
void calc_lat_fudu(int flag, int res, float *fu, float *du);
void init_latt_deform(struct Object *oblatt, struct Object *ob);
void calc_latt_deform(float *co);
void end_latt_deform(void);
int object_deform(struct Object *ob);
int object_apply_deform(struct Object *ob);
struct BPoint *latt_bp(struct Lattice *lt, int u, int v, int w);
void outside_lattice(struct Lattice *lt);


#endif

