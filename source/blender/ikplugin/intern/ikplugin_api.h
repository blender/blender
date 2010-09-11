/**
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Original author: Benoit Bolsee
 * Contributor(s): 
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef IKPLUGIN_API_H
#define IKPLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct bPoseChannel;
struct bArmature;
struct Scene;


struct IKPlugin {
	void (*initialize_tree_func)(struct Scene *scene, struct Object *ob, float ctime);
	void (*execute_tree_func)(struct Scene *scene, struct Object *ob, struct bPoseChannel *pchan, float ctime);
	void (*release_tree_func)(struct Scene *scene, struct Object *ob, float ctime);
	void (*remove_armature_func)(struct bPose *pose);
	void (*clear_cache)(struct bPose *pose);
	void (*update_param)(struct bPose *pose);
	void (*test_constraint)(struct Object *ob, struct bConstraint *cons);
};

typedef struct IKPlugin IKPlugin;

#ifdef __cplusplus
}
#endif

#endif // IKPLUGIN_API_H

