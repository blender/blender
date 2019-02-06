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

#ifndef __BKE_OBJECT_FACEMAP_H__
#define __BKE_OBJECT_FACEMAP_H__

/** \file \ingroup bke
 * \brief Functions for dealing with object face-maps.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct Object;
struct bFaceMap;

struct bFaceMap *BKE_object_facemap_add(struct Object *ob);
struct bFaceMap *BKE_object_facemap_add_name(struct Object *ob, const char *name);
void             BKE_object_facemap_remove(struct Object *ob, struct bFaceMap *fmap);
void             BKE_object_facemap_clear(struct Object *ob);

int              BKE_object_facemap_name_index(struct Object *ob, const char *name);
void             BKE_object_facemap_unique_name(struct Object *ob, struct bFaceMap *fmap);
struct bFaceMap *BKE_object_facemap_find_name(struct Object *ob, const char *name);
void             BKE_object_facemap_copy_list(struct ListBase *outbase, const struct ListBase *inbase);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_OBJECT_FACEMAP_H__ */
