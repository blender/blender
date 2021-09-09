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
 */

#pragma once

/** \file
 * \ingroup bke
 *
 * An #AnonymousAttributeID is used to identify attributes that are not explicitly named.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AnonymousAttributeID AnonymousAttributeID;

AnonymousAttributeID *BKE_anonymous_attribute_id_new_weak(const char *debug_name);
AnonymousAttributeID *BKE_anonymous_attribute_id_new_strong(const char *debug_name);
bool BKE_anonymous_attribute_id_has_strong_references(const AnonymousAttributeID *anonymous_id);
void BKE_anonymous_attribute_id_increment_weak(const AnonymousAttributeID *anonymous_id);
void BKE_anonymous_attribute_id_increment_strong(const AnonymousAttributeID *anonymous_id);
void BKE_anonymous_attribute_id_decrement_weak(const AnonymousAttributeID *anonymous_id);
void BKE_anonymous_attribute_id_decrement_strong(const AnonymousAttributeID *anonymous_id);
const char *BKE_anonymous_attribute_id_debug_name(const AnonymousAttributeID *anonymous_id);
const char *BKE_anonymous_attribute_id_internal_name(const AnonymousAttributeID *anonymous_id);

#ifdef __cplusplus
}
#endif
