/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
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
void BKE_object_facemap_remove(struct Object *ob, struct bFaceMap *fmap);
void BKE_object_facemap_clear(struct Object *ob);

int BKE_object_facemap_name_index(struct Object *ob, const char *name);
void BKE_object_facemap_unique_name(struct Object *ob, struct bFaceMap *fmap);
struct bFaceMap *BKE_object_facemap_find_name(struct Object *ob, const char *name);
void BKE_object_facemap_copy_list(struct ListBase *outbase, const struct ListBase *inbase);

int *BKE_object_facemap_index_map_create(struct Object *ob_src,
                                         struct Object *ob_dst,
                                         int *r_map_len);
void BKE_object_facemap_index_map_apply(int *fmap, int fmap_len, const int *map, int map_len);

#ifdef __cplusplus
}
#endif
