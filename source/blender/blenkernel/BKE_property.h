/*
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_PROPERTY_H__
#define __BKE_PROPERTY_H__

/** \file BKE_property.h
 *  \ingroup bke
 */

struct bProperty;
struct ListBase;
struct Object;

void free_property(struct bProperty *prop);
void free_properties(struct ListBase *lb);
struct bProperty *copy_property(struct bProperty *prop);
void copy_properties(struct ListBase *lbn, struct ListBase *lbo);
void init_property(struct bProperty *prop);
struct bProperty *new_property(int type);
void unique_property(struct bProperty *first, struct  bProperty *prop, int force);
struct bProperty *get_ob_property(struct Object *ob, const char *name);
void set_ob_property(struct Object *ob, struct bProperty *propc);
int compare_property(struct bProperty *prop, const char *str);
void set_property(struct bProperty *prop, const char *str);
void add_property(struct bProperty *prop, const char *str);
void set_property_valstr(struct bProperty *prop, char *str);
void cp_property(struct bProperty *prop1, struct bProperty *prop2);
	
#endif
