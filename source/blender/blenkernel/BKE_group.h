/**
 * blenlib/BKE_group.h (mar-2001 nzc)
 *	
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
#ifndef BKE_GROUP_H
#define BKE_GROUP_H

struct Group;
struct GroupKey;
struct GroupObject;
struct ObjectKey;
struct Object;

void free_object_key(struct ObjectKey *ok);
void free_group_object(struct GroupObject *go);
void free_group(struct Group *group);
struct Group *add_group(void);
void object_to_obkey(struct Object *ob, struct ObjectKey *ok);
void obkey_to_object(struct ObjectKey *ok, struct Object *ob);
void add_object_key(struct GroupObject *go, struct GroupKey *gk);
void add_to_group(struct Group *group, struct Object *ob);
void rem_from_group(struct Group *group, struct Object *ob);
void add_group_key(struct Group *group);
void set_object_key(struct Object *ob, struct ObjectKey *ok);
void set_group_key(struct Group *group);
struct Group *find_group(struct Object *ob);
void set_group_key_name(struct Group *group, char *name);
void set_group_key_frame(struct Group *group, float frame);

#endif

