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
 */

#ifndef BIF_DEFORM_H
#define BIF_DEFORM_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct Object;
struct MDeformVert;
struct MDeformWeight;
struct bDeformGroup;

void unique_vertexgroup_name (struct bDeformGroup *dg, struct Object *ob);
void add_defgroup (struct Object *ob);
void del_defgroup (struct Object *ob);
void assign_verts_defgroup (void);
void remove_verts_defgroup (int allverts);
void sel_verts_defgroup (int select);
struct MDeformWeight *verify_defweight (struct MDeformVert *dv, int defgroup);
void verify_defgroups (struct Object *ob);

#endif

