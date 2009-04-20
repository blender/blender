/**
 * blenkernel/BKE_property.h (mar-2001 nzc)
 *
 * $Id$ 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_PROPERTY_H
#define BKE_PROPERTY_H

struct bProperty;
struct ListBase;
struct Object;

void free_property(struct bProperty *prop);
void free_properties(struct ListBase *lb);
struct bProperty *copy_property(struct bProperty *prop);
void copy_properties(struct ListBase *lbn, struct ListBase *lbo);
void init_property(struct bProperty *prop);
struct bProperty *new_property(int type);
struct bProperty *get_ob_property(struct Object *ob, char *name);
void set_ob_property(struct Object *ob, struct bProperty *propc);
int compare_property(struct bProperty *prop, char *str);
void set_property(struct bProperty *prop, char *str);
void add_property(struct bProperty *prop, char *str);
void set_property_valstr(struct bProperty *prop, char *str);
void cp_property(struct bProperty *prop1, struct bProperty *prop2);
	
#endif

