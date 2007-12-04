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

#ifndef BIF_POSEOBJECT
#define BIF_POSEOBJECT


struct Object;
struct bPoseChannel;

void enter_posemode(void);
void exit_posemode(void);

 // sets chan->flag to POSE_KEY if bone selected
void set_pose_keys(struct Object *ob);

struct bPoseChannel *get_active_posechannel (struct Object *ob);
int pose_channel_in_IK_chain(struct Object *ob, struct bPoseChannel *pchan);

/* tools */
void pose_select_constraint_target(void);
void pose_special_editmenu(void);
void pose_add_IK(void);
void pose_clear_IK(void);
void pose_clear_constraints(void);
void pose_copy_menu(void);

void free_posebuf(void);
void copy_posebuf (void);
void paste_posebuf (int flip);

void pose_adds_vgroups(struct Object *meshobj, int heatweights);

void pose_calculate_path(struct Object *ob);
void pose_recalculate_paths(struct Object *ob);
void pose_clear_paths(struct Object *ob);

void pose_flip_names(void);
void pose_activate_flipped_bone(void);
void pose_movetolayer(void);

#endif

