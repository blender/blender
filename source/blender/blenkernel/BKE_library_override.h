/*
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
 * The Original Code is Copyright (C) 2016 by Blender Foundation.
 * All rights reserved.
 */

#ifndef __BKE_LIBRARY_OVERRIDE_H__
#define __BKE_LIBRARY_OVERRIDE_H__

/** \file \ingroup bke
 */

struct ID;
struct IDOverrideStatic;
struct IDOverrideStaticProperty;
struct IDOverrideStaticPropertyOperation;
struct Main;

void BKE_override_static_enable(const bool do_enable);
bool BKE_override_static_is_enabled(void);

struct IDOverrideStatic *BKE_override_static_init(struct ID *local_id, struct ID *reference_id);
void BKE_override_static_copy(struct ID *dst_id, const struct ID *src_id);
void BKE_override_static_clear(struct IDOverrideStatic *override);
void BKE_override_static_free(struct IDOverrideStatic **override);

struct ID *BKE_override_static_create_from_id(struct Main *bmain, struct ID *reference_id);
bool BKE_override_static_create_from_tag(struct Main *bmain);

struct IDOverrideStaticProperty *BKE_override_static_property_find(struct IDOverrideStatic *override, const char *rna_path);
struct IDOverrideStaticProperty *BKE_override_static_property_get(struct IDOverrideStatic *override, const char *rna_path, bool *r_created);
void BKE_override_static_property_delete(struct IDOverrideStatic *override, struct IDOverrideStaticProperty *override_property);

struct IDOverrideStaticPropertyOperation *BKE_override_static_property_operation_find(
        struct IDOverrideStaticProperty *override_property,
        const char *subitem_refname, const char *subitem_locname,
        const int subitem_refindex, const int subitem_locindex, const bool strict, bool *r_strict);
struct IDOverrideStaticPropertyOperation *BKE_override_static_property_operation_get(
        struct IDOverrideStaticProperty *override_property, const short operation,
        const char *subitem_refname, const char *subitem_locname,
        const int subitem_refindex, const int subitem_locindex,
        const bool strict, bool *r_strict, bool *r_created);
void BKE_override_static_property_operation_delete(
        struct IDOverrideStaticProperty *override_property, struct IDOverrideStaticPropertyOperation *override_property_operation);

bool BKE_override_static_status_check_local(struct Main *bmain, struct ID *local);
bool BKE_override_static_status_check_reference(struct Main *bmain, struct ID *local);

bool BKE_override_static_operations_create(struct Main *bmain, struct ID *local, const bool force_auto);
void BKE_main_override_static_operations_create(struct Main *bmain, const bool force_auto);

void BKE_override_static_update(struct Main *bmain, struct ID *local);
void BKE_main_override_static_update(struct Main *bmain);


/* Storage (.blend file writing) part. */

/* For now, we just use a temp main list. */
typedef struct Main OverrideStaticStorage;

OverrideStaticStorage *BKE_override_static_operations_store_initialize(void);
struct ID *BKE_override_static_operations_store_start(
        struct Main *bmain, OverrideStaticStorage *override_storage, struct ID *local);
void BKE_override_static_operations_store_end(OverrideStaticStorage *override_storage, struct ID *local);
void BKE_override_static_operations_store_finalize(OverrideStaticStorage *override_storage);



#endif  /* __BKE_LIBRARY_OVERRIDE_H__ */
