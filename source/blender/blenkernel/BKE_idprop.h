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
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef __BKE_IDPROP_H__
#define __BKE_IDPROP_H__

/** \file BKE_idprop.h
 *  \ingroup bke
 *  \author Joseph Eagar
 */

#include "DNA_ID.h"

struct IDProperty;
struct ID;

typedef union IDPropertyTemplate {
	int i;
	float f;
	double d;
	struct {
		char *str;
		short len;
		char subtype;
	} string;
	struct ID *id;
	struct {
		short type;
		short len;
	} array;
	struct {
		int matvec_size;
		float *example;
	} matrix_or_vector;
} IDPropertyTemplate;

/* ----------- Property Array Type ---------- */

IDProperty *IDP_NewIDPArray(const char *name)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
IDProperty *IDP_CopyIDPArray(IDProperty *array)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

void IDP_FreeIDPArray(IDProperty *prop);

/* shallow copies item */
void IDP_SetIndexArray(struct IDProperty *prop, int index, struct IDProperty *item);
#ifdef __GNUC__
__attribute__((nonnull))
#endif
struct IDProperty *IDP_GetIndexArray(struct IDProperty *prop, int index)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
void IDP_AppendArray(struct IDProperty *prop, struct IDProperty *item);
void IDP_ResizeIDPArray(struct IDProperty *prop, int len);

/* ----------- Numeric Array Type ----------- */
/*this function works for strings too!*/
void IDP_ResizeArray(struct IDProperty *prop, int newlen);
void IDP_FreeArray(struct IDProperty *prop);

/* ---------- String Type ------------ */
IDProperty *IDP_NewString(const char *st, const char *name, int maxlen) /* maxlen excludes '\0' */
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull (2))) /* 'name' arg */
#endif
;

void IDP_AssignString(struct IDProperty *prop, const char *st, int maxlen) /* maxlen excludes '\0' */
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;
void IDP_ConcatStringC(struct IDProperty *prop, const char *st)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;
void IDP_ConcatString(struct IDProperty *str1, struct IDProperty *append)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;
void IDP_FreeString(struct IDProperty *prop)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

/*-------- ID Type -------*/
void IDP_LinkID(struct IDProperty *prop, ID *id);
void IDP_UnlinkID(struct IDProperty *prop);

/*-------- Group Functions -------*/

/** Sync values from one group to another, only where they match */
void IDP_SyncGroupValues(struct IDProperty *dest, struct IDProperty *src)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

void IDP_ReplaceGroupInGroup(struct IDProperty *dest, struct IDProperty *src)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;
void IDP_ReplaceInGroup(struct IDProperty *group, struct IDProperty *prop)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

void IDP_MergeGroup(IDProperty *dest, IDProperty *src, const int do_overwrite)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

int IDP_AddToGroup(struct IDProperty *group, struct IDProperty *prop)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;
int IDP_InsertToGroup(struct IDProperty *group, struct IDProperty *previous, 
                      struct IDProperty *pnew)
#ifdef __GNUC__
__attribute__((nonnull  (1, 3))) /* 'group', 'pnew' */
#endif
;
void IDP_RemFromGroup(struct IDProperty *group, struct IDProperty *prop)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

IDProperty *IDP_GetPropertyFromGroup(struct IDProperty *prop, const char *name)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
IDProperty *IDP_GetPropertyTypeFromGroup(struct IDProperty *prop, const char *name, const char type)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
void *IDP_GetGroupIterator(struct IDProperty *prop)
#ifdef __GNUC__
__attribute__((warn_unused_result))
#endif
;
IDProperty *IDP_GroupIterNext(void *vself)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
void IDP_FreeIterBeforeEnd(void *vself)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

/*-------- Main Functions --------*/
struct IDProperty *IDP_GetProperties(struct ID *id, int create_if_needed)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
struct IDProperty *IDP_CopyProperty(struct IDProperty *prop)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

int IDP_EqualsProperties_ex(IDProperty *prop1, IDProperty *prop2, const int is_strict)
#ifdef __GNUC__
__attribute__((warn_unused_result))
#endif
;

int IDP_EqualsProperties(struct IDProperty *prop1, struct IDProperty *prop2)
#ifdef __GNUC__
__attribute__((warn_unused_result))
#endif
;

struct IDProperty *IDP_New(const int type, const IDPropertyTemplate *val, const char *name)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

void IDP_FreeProperty(struct IDProperty *prop);

void IDP_ClearProperty(IDProperty *prop);

void IDP_UnlinkProperty(struct IDProperty *prop);

#define IDP_Int(prop)                     ((prop)->data.val)
#define IDP_Float(prop)        (*(float *)&(prop)->data.val)
#define IDP_Double(prop)      (*(double *)&(prop)->data.val)
#define IDP_String(prop)         ((char *) (prop)->data.pointer)
#define IDP_Array(prop)                   ((prop)->data.pointer)
#define IDP_IDPArray(prop) ((IDProperty *) (prop)->data.pointer)

#ifdef DEBUG
/* for printout only */
void IDP_spit(IDProperty *prop);
#endif

#endif /* __BKE_IDPROP_H__ */
