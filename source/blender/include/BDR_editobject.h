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

struct Object;
struct Tex;
struct Material;
struct Base;
struct HookModifierData;
struct Scene;

void add_object_draw(int type);
void add_objectLamp(short type);
void free_and_unlink_base_from_scene(struct Scene *scene, struct Base *base);
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
void make_proxy(void);

#define EM_WAITCURSOR	(1 << 0)
#define EM_FREEDATA 	(1 << 1)
#define EM_FREEUNDO 	(1 << 2)

void exit_editmode(int flag);
void check_editmode(int type);
void enter_editmode(int wc);

void exit_paint_modes(void);

void docenter(int centermode);
void docenter_new(void);
void docenter_cursor(void);
void movetolayer(void);
void special_editmenu(void);
void convertmenu(void);
void copy_attr_menu(void);
void copy_attr(short event);
void link_to_scene(unsigned short nr);
void make_links_menu(void);
void make_links(short event);
void make_duplilist_real(void);
void apply_objects_locrot(void);
void apply_objects_visual_tx(void);
void apply_object(void);

/* old transform */
void apply_keyb_grid(float *val, float fac1, float fac2, float fac3, int invert);
void headerprint(char *str);
/* used for old game engine collision optimize */
int cylinder_intersect_test(void);
int sphere_intersect_test(void);


void std_rmouse_transform(void (*xf_func)(int, int));
void rightmouse_transform(void);
void single_object_users(int flag);
void new_id_matar(struct Material **matar, int totcol);
void single_obdata_users(int flag);
void single_mat_users(int flag);
void do_single_tex_user(struct Tex **from);
void single_tex_users_expand(void);
void single_mat_users_expand(void);
void single_user(void);
void make_local_menu(void);
void make_local(int mode);
void adduplicate(int mode, int dupflag); /* when the dupflag is 0 no data is duplicated */
void selectlinks_menu(void);
void selectlinks(int nr);
void image_aspect(void);
void set_ob_ipoflags(void);
void select_select_keys(void);
int vergbaseco(const void *a1, const void *a2);
void auto_timeoffs(void);
void texspace_edit(void);
void flip_subdivison(int);
void mirrormenu(void);
void hookmenu(void); /* object mode hook menu */


void add_hook(void);
void hook_select(struct HookModifierData *hmd);
int hook_getIndexArray(int *tot, int **indexar, char *name, float *cent_r);

int object_is_libdata(struct Object *ob);
int object_data_is_libdata(struct Object *ob);	
void hide_objects(int select);
void show_objects(void);

#endif /*  BDR_EDITOBJECT_H */

