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

#ifndef BSE_EDITIPO_H
#define BSE_EDITIPO_H

struct TransVert;
struct IpoCurve;
struct BezTriple;
struct Ipo;
struct EditIpo;
struct ID;
struct ListBase;
struct Object;
struct IpoKey;
struct TransOb;

void remake_object_ipos(struct Object *ob);
void getname_ac_ei(int nr, char *str);
void getname_co_ei(int nr, char *str);
void getname_ob_ei(int nr, char *str, int colipo);
void getname_tex_ei(int nr, char *str);
void getname_mat_ei(int nr, char *str);
void getname_world_ei(int nr, char *str);
void getname_seq_ei(int nr, char *str);
void getname_cu_ei(int nr, char *str);
void getname_key_ei(int nr, char *str);
void getname_la_ei(int nr, char *str);
void getname_cam_ei(int nr, char *str);
void getname_snd_ei(int nr, char *str);
struct IpoCurve *find_ipocurve(struct Ipo *ipo, int adrcode);
void boundbox_ipocurve(struct IpoCurve *icu);
void boundbox_ipo(struct Ipo *ipo, struct rctf *bb);
void editipo_changed(struct SpaceIpo *si, int doredraw);
void scale_editipo(void);
struct Ipo *get_ipo_to_edit(struct ID **from);
unsigned int ipo_rainbow(int cur, int tot);
void make_ob_editipo(struct Object *ob, struct SpaceIpo *si);
void make_seq_editipo(struct SpaceIpo *si);
void make_cu_editipo(struct SpaceIpo *si);
void make_key_editipo(struct SpaceIpo *si);
int texchannel_to_adrcode(int channel);
void make_mat_editipo(struct SpaceIpo *si);
void make_world_editipo(struct SpaceIpo *si);
void make_lamp_editipo(struct SpaceIpo *si);
void make_camera_editipo(struct SpaceIpo *si);
int make_action_editipo(struct Ipo *ipo, struct EditIpo **si);
int make_constraint_editipo(struct Ipo *ipo, struct EditIpo **si);
void make_sound_editipo(struct SpaceIpo *si);
void make_editipo(void);
void test_editipo(void);
void get_status_editipo(void);
void update_editipo_flags(void);
void set_editflag_editipo(void);
void ipo_toggle_showkey(void);
void swap_selectall_editipo(void);
void swap_visible_editipo(void);
void deselectall_editipo(void);
short findnearest_ipovert(struct IpoCurve **icu, struct BezTriple **bezt);
void move_to_frame(void);
void do_ipowin_buts(short event);
void do_ipo_selectbuttons(void);
struct EditIpo *get_editipo(void);
struct Ipo *get_ipo(struct ID *from, short type, int make);
struct IpoCurve *get_ipocurve(struct ID *from, short type, int adrcode, struct Ipo* useipo);
void insert_vert_ipo(struct IpoCurve *icu, float x, float y);
void add_vert_ipo(void);
void add_duplicate_editipo(void);
void remove_doubles_ipo(void);
void join_ipo_menu(void);
void join_ipo(int mode);
void ipo_snap_menu(void);
void ipo_snap(short event);
void mouse_select_ipo(void);
void sethandles_ipo(int code);
void select_ipo_bezier_keys(struct Ipo *ipo, int selectmode);
void set_ipotype(void);
void borderselect_ipo(void);
void del_ipo(void);
void free_ipocopybuf(void);
void copy_editipo(void);
void paste_editipo(void);
void set_exprap_ipo(int mode);
int find_other_handles(struct EditIpo *eicur, 
					   float ctime, struct BezTriple **beztar);
void set_speed_editipo(float speed);
void insertkey(struct ID *id, int adrcode);
void insertkey_editipo(void);
void common_insertkey(void);
void free_ipokey(struct ListBase *lb);
void add_to_ipokey(struct ListBase *lb, struct BezTriple *bezt, int nr, int len);
void make_ipokey(void);
void make_ipokey_spec(struct ListBase *lb, struct Ipo *ipo);
void make_ipokey_transform(struct Object *ob, struct ListBase *lb, int sel);
void update_ipokey_val(void);
void set_tob_old(float *old, float *poin);
void set_ipo_pointers_transob(struct IpoKey *ik, struct TransOb *tob);
void nextkey(struct ListBase *elems, int dir);
void movekey_ipo(int dir);
void movekey_obipo(int dir);
void nextkey_ipo(int dir);
void nextkey_obipo(int dir);
void remake_ipo_transverts(struct TransVert *transmain, float *dvec, int tot);
void transform_ipo(int mode);
void clever_numbuts_ipo(void);
void filter_sampledata(float *data, int sfra, int efra);
void sampledata_to_ipocurve(float *data, int sfra, int efra, struct IpoCurve *icu);
void ipo_record(void);    

void sethandles_ipo_keys(struct Ipo *ipo, int code);
void setipotype_ipo(struct Ipo *ipo, int code);
void set_ipo_key_selection(struct Ipo *ipo, int sel);
int is_ipo_key_selected(struct Ipo *ipo);
void delete_ipo_keys(struct Ipo *ipo);
int fullselect_ipo_keys(struct Ipo *ipo);
int add_trans_ipo_keys(struct Ipo *ipo, struct TransVert *tv, int tvtot);
void duplicate_ipo_keys(struct Ipo *ipo);
void borderselect_ipo_key(struct Ipo *ipo, float xmin, float xmax, int val);
void borderselect_icu_key(struct IpoCurve *icu, float xmin, float xmax, 
						  int (*select_function)(struct BezTriple *));
void select_ipo_key(struct Ipo *ipo, float selx, int sel);
void select_icu_key(struct IpoCurve *icu, float selx, int selectmode);
int select_bezier_add(struct BezTriple *bezt);
int select_bezier_subtract(struct BezTriple *bezt);
int select_bezier_invert(struct BezTriple *bezt);

#endif /*  BSE_EDITIPO_H */

