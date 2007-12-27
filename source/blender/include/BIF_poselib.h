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
 * The Original Code is Copyright (C) 2007 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: this is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
 
#ifndef BIF_POSELIB_H
#define BIF_POSELIB_H

struct Object;
struct bAction;
struct bPoseLib;
struct bPoseLibRef;

char *poselib_build_poses_menu(struct bPoseLib *pl, char title[]);
void poselib_unique_pose_name(struct bPoseLib *pl, char name[]);
int poselib_get_free_index(struct bPoseLib *pl);

struct bPoseLib *poselib_init_new(struct Object *ob);
void poselib_validate_act(struct bAction *act);

void poselib_remove_pose(struct Object *ob, struct bPoseLibRef *plr);
void poselib_rename_pose(struct Object *ob);
void poselib_add_current_pose(struct Object *ob, int mode);

void poselib_preview_poses(struct Object *ob);

#endif
