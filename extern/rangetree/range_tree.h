/*
 * Copyright (c) 2016, Campbell Barton.
 *
 * Licensed under the Apache License, Version 2.0 (the "Apache License")
 * with the following modification; you may not use this file except in
 * compliance with the Apache License and the following modification to it:
 * Section 6. Trademarks. is deleted and replaced with:
 *
 * 6. Trademarks. This License does not grant permission to use the trade
 *   names, trademarks, service marks, or product names of the Licensor
 *   and its affiliates, except as required to comply with Section 4(c) of
 *   the License and to reproduce the content of the NOTICE file.
 *
 * You may obtain a copy of the Apache License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Apache License with the above modification is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the Apache License for the specific
 * language governing permissions and limitations under the Apache License.
 */

#ifndef __RANGE_TREE_H__
#define __RANGE_TREE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RangeTreeUInt RangeTreeUInt;

struct RangeTreeUInt *range_tree_uint_alloc(unsigned int min, unsigned int max);
void                  range_tree_uint_free(struct RangeTreeUInt *rt);
struct RangeTreeUInt *range_tree_uint_copy(const struct RangeTreeUInt *rt_src);

bool         range_tree_uint_has(struct RangeTreeUInt *rt, const unsigned int value);
void         range_tree_uint_take(struct RangeTreeUInt *rt, const unsigned int value);
bool         range_tree_uint_retake(struct RangeTreeUInt *rt, const unsigned int value);
unsigned int range_tree_uint_take_any(struct RangeTreeUInt *rt);
void         range_tree_uint_release(struct RangeTreeUInt *rt, const unsigned int value);

#ifdef __cplusplus
}
#endif

#endif /* __RANGE_TREE_H__ */
