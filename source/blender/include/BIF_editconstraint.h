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

#ifndef BIF_EDITCONSTRAINT_H
#define BIF_EDITCONSTRAINT_H

struct ListBase;
struct Object;
struct bConstraint;
struct bConstraintChannel;

typedef struct ConstraintElement{
	struct ConstraintElement *next, *prev;
	struct ConstraintElement *parent;
	Object		*ob;
	int			flag;
	const char	*substring;
	void		*subdata;
} ConstraintElement;

struct bConstraintChannel *add_new_constraint_channel(const char *name);
struct bConstraint * add_new_constraint(char type);
void add_influence_key_to_constraint (struct bConstraint *con);
void add_constraint_to_object(struct bConstraint *con, struct Object *ob);
void add_constraint_to_client(struct bConstraint *con);
struct ListBase *get_constraint_client_channels (int forcevalid);
struct ListBase *get_constraint_client(char *name, short *clienttype, void** clientdata);
int test_constraints (struct Object *owner, const char *substring, int disable);
void test_scene_constraints (void);

/*  void unique_constraint_name (struct bConstraint *con, struct ListBase *list); */

#endif

