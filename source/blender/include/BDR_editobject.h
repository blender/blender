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

#ifndef BDR_EDITOBJECT_H
#define BDR_EDITOBJECT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct TransVert;
struct Object;
struct TransOb;
struct Tex;
struct Material;
struct Base;

void add_object_draw(int type);
void free_and_unlink_base(struct Base *base);
void delete_obj(int ok);
void make_track(void);
void apply_obmat(struct Object *ob);
void clear_parent(void);
void clear_track(void);
void clear_object(char mode);
void reset_slowparents(void);
void set_slowparent(void);
void make_vertex_parent(void);
int test_parent_loop(struct Object *par, struct Object *ob);
void make_parent(void);

void make_displists_by_parent(struct Object *ob);
void exit_editmode(int freedata);
void check_editmode(int type);
void docentre(void);
void docentre_new(void);
void docentre_cursor(void);
void movetolayer(void);
void special_editmenu(void);
void convertmenu(void);
void copymenu_properties(struct Object *ob);
void copymenu(void);
void link_to_scene(unsigned short nr);
void linkmenu(void);
void make_duplilist_real(void);
void apply_object(void);
void ob_to_transob(struct Object *ob, struct TransOb *tob);
void ob_to_tex_transob(struct Object *ob, struct TransOb *tob);
void make_trans_objects(void);
void enter_editmode(void);
void copymenu_logicbricks(struct Object *ob);
void clearbaseflags_for_editing(void);
void make_trans_verts(float *min, float *max, int mode);
void draw_prop_circle(void);
void set_proportional_weight(struct TransVert *tv, float *min, float *max);
void special_trans_update(int keyflags);
void special_aftertrans_update(char mode, int flip, short canceled, int keyflags);
void calc_trans_verts(void);
void apply_keyb_grid(float *val, float fac1, float fac2, float fac3, int invert);
void compatible_eul(float *eul, float *oldrot);
void headerprint(char *str);
void add_ipo_tob_poin(float *poin, float *old, float delta);
void restore_tob(struct TransOb *tob);
int cylinder_intersect_test(void);
int sphere_intersect_test(void);
int my_clock(void);
void transform(int mode);
void std_rmouse_transform(void (*xf_func)(int));
void rightmouse_transform(void);
void single_object_users(int flag);
void new_id_matar(struct Material **matar, int totcol);
void single_obdata_users(int flag);
void single_mat_users(int flag);
void do_single_tex_user(struct Tex **from);
void single_tex_users_expand(void);
void single_mat_users_expand(void);
void single_user(void);
void make_local(void);
void adduplicate(float *dtrans);
void selectlinks(void);
void image_aspect(void);
void set_ob_ipoflags(void);
void select_select_keys(void);
int verg_hoogste_zco(const void *a1, const void *a2);
void sortfaces(void);
int vergbaseco(const void *a1, const void *a2);
void auto_timeoffs(void);
void texspace_edit(void);
void first_base(void);
void make_displists_by_obdata(void *obdata);

#endif /*  BDR_EDITOBJECT_H */

