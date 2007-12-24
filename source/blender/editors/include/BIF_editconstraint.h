/**
 * $Id: BIF_editconstraint.h 12339 2007-10-22 10:49:34Z aligorith $
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

struct ID;
struct ListBase;
struct Object;
struct bConstraint;
struct bConstraintChannel;
struct Text;

/* generic constraint editing functions */

struct bConstraint *add_new_constraint(short type);

void add_constraint_to_object(struct bConstraint *con, struct Object *ob);

struct ListBase *get_active_constraints(struct Object *ob);
struct bConstraint *get_active_constraint(struct Object *ob);
struct ListBase *get_active_constraint_channels (struct Object *ob, int forcevalid);
struct bConstraintChannel *get_active_constraint_channel(struct Object *ob);

void object_test_constraints(struct Object *owner);

void add_constraint(int only_IK);
void ob_clear_constraints(void);

void rename_constraint(struct Object *ob, struct bConstraint *con, char *newname);


/* a few special functions for PyConstraints */
char *buildmenu_pyconstraints(struct Text *con_text, int *pyconindex);
void validate_pyconstraint_cb(void *arg1, void *arg2);
void update_pyconstraint_cb(void *arg1, void *arg2);

/* two special functions for ChildOf Constriant */
void childof_const_setinv (void *conv, void *unused);
void childof_const_clearinv(void *conv, void *unused);

#endif

