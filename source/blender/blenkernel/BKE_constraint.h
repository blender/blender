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

#ifndef BKE_CONSTRAINT_H
#define BKE_CONSTRAINT_H

struct bConstraint;
struct Object;
struct ListBase;
struct bConstraintChannel;
struct bAction;
struct bArmature;

/* Function prototypes */
void evaluate_constraint (struct bConstraint *constraint, struct Object *ob, short ownertype, void *ownerdata, float targetmat[][4]);
void free_constraints (struct ListBase *conlist);
void copy_constraints (struct ListBase *dst, struct ListBase *src);
void *copy_constraint_channels (ListBase *dst, ListBase *src);
struct bConstraintChannel *clone_constraint_channels (struct ListBase *dst, struct ListBase *src, struct bConstraintChannel *oldact);
void relink_constraints (struct ListBase *list);
void free_constraint_data (struct bConstraint *con);
void clear_object_constraint_status (struct Object *ob);
void do_constraint_channels (struct ListBase *conbase, struct ListBase *chanbase, float ctime);
short get_constraint_target (struct bConstraint *con, short ownertype, void *ownerdata, float mat[][4], float size[3], float time);
struct bConstraintChannel *find_constraint_channel (ListBase *list, const char *name);
void free_constraint_channels (ListBase *chanbase);

/* Constraint target/owner types */
#define TARGET_OBJECT			1	//	string is ""
#define TARGET_BONE				2	//	string is bone-name
#define TARGET_VERT				3	//	string is "VE:#" 
#define TARGET_FACE				4	//	string is "FA:#" 
#define TARGET_CV				5	//	string is "CV:#"

#endif
