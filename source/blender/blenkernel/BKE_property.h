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

void              BKE_bproperty_free(struct bProperty *prop);
void              BKE_bproperty_free_list(struct ListBase *lb);
struct bProperty *BKE_bproperty_copy(struct bProperty *prop);
void              BKE_bproperty_copy_list(struct ListBase *lbn, struct ListBase *lbo);
void              BKE_bproperty_init(struct bProperty *prop);
struct bProperty *BKE_bproperty_new(int type);
void              BKE_bproperty_unique(struct bProperty *first, struct  bProperty *prop, int force);
struct bProperty *BKE_bproperty_object_get(struct Object *ob, const char *name);
void              BKE_bproperty_object_set(struct Object *ob, struct bProperty *propc);
// int               BKE_bproperty_cmp(struct bProperty *prop, const char *str);
void              BKE_bproperty_set(struct bProperty *prop, const char *str);
void              BKE_bproperty_add(struct bProperty *prop, const char *str);
/* should really be called '_get_valstr()' or '_as_string()' */
void              BKE_bproperty_set_valstr(struct bProperty *prop, char str[MAX_PROPSTRING]);
	
#endif
