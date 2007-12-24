/**
 * $Id: BSE_headerbuttons.h 10893 2007-06-08 14:17:13Z jiri $
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

#ifndef BSE_HEADERBUTTONS_H
#define BSE_HEADERBUTTONS_H

struct uiBlock;
struct ScrArea;
struct ID;
struct SpaceIpo;
struct Ipo;

/* these used to be in blender/src/headerbuttons.c: */
#define XIC 20
#define YIC 20

int std_libbuttons(struct uiBlock *block, 
       short xco, short yco, int pin, short *pinpoin, 
       int browse, short id_code, short special, struct ID *id, struct ID *parid, 
       short *menupoin, int users, 
       int lib, int del, int autobut, int keepbut);

char *windowtype_pup(void);

int GetButStringLength(char *str);
void load_space_sound(char *str);
void load_sound_buttons(char *str);
/* end of declarations moved from old headerbuttons.c */

void update_for_newframe_muted(void);
void update_for_newframe_nodraw(int nosound);
void free_matcopybuf(void);
void clear_matcopybuf(void);
void write_vrml_fs(void);
void write_dxf_fs(void);
void write_stl_fs(void);
int buttons_do_unpack(void);
struct Scene *copy_scene(struct Scene *sce, int level);

void buttons_active_id(struct ID **id, struct ID **idfrom);

int start_progress_bar(void);
void end_progress_bar(void);
int progress_bar(float done, char *busy_info);

void update_for_newframe(void);

void action_buttons(void);
void buts_buttons(void);
void file_buttons(void);
void image_buttons(void);
void imasel_buttons(void);
void info_buttons(void);
void ipo_buttons(void);
void nla_buttons(void);
void oops_buttons(void);
void seq_buttons(void);
void sound_buttons(void);
void text_buttons(void);
void script_buttons(void);
void view3d_buttons(void);
void time_buttons(struct ScrArea *sa);
void node_buttons(struct ScrArea *sa);

void do_global_buttons(unsigned short event);
void do_global_buttons2(short event);

void do_action_buttons(unsigned short event);
void do_buts_buttons(short event);
void do_file_buttons(short event);
void do_image_buttons(unsigned short event);
void do_imasel_buttons(short event);
void do_info_buttons(unsigned short event);
void do_ipo_buttons(short event);
void do_layer_buttons(short event);
void do_nla_buttons(unsigned short event);
void do_oops_buttons(short event);
void do_seq_buttons(short event);
void do_sound_buttons(unsigned short event);
void do_text_buttons(unsigned short event);
void do_script_buttons(unsigned short event);
void do_view3d_buttons(short event); 
void do_time_buttons(struct ScrArea *sa, unsigned short event);
void do_node_buttons(struct ScrArea *sa, unsigned short event); 

void do_headerbuttons(short event);

/* header_ipo.c */
void spaceipo_assign_ipo(struct SpaceIpo *si, struct Ipo *ipo);

/* header_text.c */
void do_text_editmenu_to3dmenu(void *arg, int event);
void do_text_formatmenu_convert(void *arg, int event);

/* header_info.c */
void do_info_add_meshmenu(void *arg, int event);
void do_info_add_curvemenu(void *arg, int event);
void do_info_add_surfacemenu(void *arg, int event);
void do_info_add_metamenu(void *arg, int event);
void do_info_add_lampmenu(void *arg, int event);
void do_info_addmenu(void *arg, int event);

/* header_node.c */
void do_node_addmenu(void *arg, int event);

/* header_view3d.c */
void do_view3d_select_objectmenu(void *arg, int event);
void do_view3d_select_object_groupedmenu(void *arg, int event);
void do_view3d_select_object_linkedmenu(void *arg, int event);
void do_view3d_select_object_layermenu(void *arg, int event);
void do_view3d_select_object_typemenu(void *arg, int event);
void do_view3d_select_faceselmenu(void *arg, int event);
void do_view3d_select_meshmenu(void *arg, int event);
void do_view3d_select_curvemenu(void *arg, int event);
void do_view3d_select_metaballmenu(void *arg, int event);
void do_view3d_edit_snapmenu(void *arg, int event);
void do_view3d_edit_mirrormenu(void *arg, int event);
void do_view3d_transform_moveaxismenu(void *arg, int event);
void do_view3d_transform_rotateaxismenu(void *arg, int event);
void do_view3d_transform_scaleaxismenu(void *arg, int event);
void do_view3d_object_mirrormenu(void *arg, int event);
void do_view3d_edit_mesh_normalsmenu(void *arg, int event);
void do_view3d_edit_mesh_verticesmenu(void *arg, int event);
void do_view3d_edit_mesh_edgesmenu(void *arg, int event);
void do_view3d_edit_mesh_facesmenu(void *arg, int event);
void do_view3d_edit_curve_segmentsmenu(void *arg, int event);
void do_view3d_edit_curve_showhidemenu(void *arg, int event); 

#endif /*  BSE_HEADERBUTTONS_H */
