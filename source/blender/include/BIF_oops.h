/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BIF_OOPS_H
#define BIF_OOPS_H

struct Curve;
struct Oops;
struct OopsLink;
struct SpaceOops;
struct Material;
struct Mesh;
struct MetaBall;
struct Object;
struct Lamp;
struct Camera;
struct Texture;
struct Lattice;
struct bArmature;
void add_curve_oopslinks(struct Curve *cu, struct Oops *oops, short flag);
void add_from_link(struct Oops *from, struct Oops *oops);
void add_material_oopslinks(struct Material *ma, struct Oops *oops, short flag);
void add_mball_oopslinks(struct MetaBall *mb, struct Oops *oops, short flag);
void add_mesh_oopslinks(struct Mesh *me, struct Oops *oops, short flag);
void add_object_oopslinks(struct Object *ob, struct Oops *oops, short flag);
void add_lamp_oopslinks(struct Lamp *la, struct Oops *oops, short flag);
void add_camera_oopslinks(struct Camera *ca, struct Oops *oops, short flag);
void add_texture_oopslinks(struct Tex *tex, struct Oops *oops, short flag);
void add_lattice_oopslinks(struct Lattice *lt, struct Oops *oops, short flag);
struct Oops *add_oops(void *id);
struct OopsLink *add_oopslink(char *name, struct Oops *oops, short type, void *from, float xof, float yof);
struct Oops *add_test_oops(void *id);	/* incl links */
void add_texture_oops(struct Material *ma);
void build_oops(void);
struct Oops *find_oops(ID *id);
void free_oops(struct Oops *oops);	/* ook oops zelf */
void free_oopspace(struct SpaceOops *so);
void new_oops_location(struct Oops *);
int oops_test_overlap(struct Oops *test);
int oops_test_overlaphide(struct Oops *test);
float oopslink_totlen(struct Oops *oops);
void shrink_oops(void);
void shuffle_oops(void);
int test_oops(struct Oops *oops);
void test_oopslink(struct OopsLink *ol);
void test_oopslinko(struct OopsLink *ol);

#endif


