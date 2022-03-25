/* SPDX-License-Identifier: GPL-2.0-or-later */

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
