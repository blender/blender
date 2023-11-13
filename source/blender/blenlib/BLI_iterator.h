/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BLI_Iterator {
  void *current; /* current pointer we iterate over */
  void *data;    /* stored data required for this iterator */
  bool skip;
  bool valid;
} BLI_Iterator;

typedef void (*IteratorCb)(BLI_Iterator *iter);
typedef void (*IteratorBeginCb)(BLI_Iterator *iter, void *data_in);

#define BLI_ITERATOR_INIT(iter) \
  { \
    (iter)->skip = false; \
    (iter)->valid = true; \
  } \
  ((void)0)

#define ITER_BEGIN(callback_begin, callback_next, callback_end, _data_in, _type, _instance) \
  { \
    _type _instance; \
    IteratorCb callback_end_func = callback_end; \
    BLI_Iterator iter_macro; \
    BLI_ITERATOR_INIT(&iter_macro); \
    for (callback_begin(&iter_macro, (_data_in)); iter_macro.valid; callback_next(&iter_macro)) { \
      if (iter_macro.skip) { \
        iter_macro.skip = false; \
        continue; \
      } \
      _instance = (_type)iter_macro.current;

#define ITER_END \
  } \
  callback_end_func(&iter_macro); \
  } \
  ((void)0)

#ifdef __cplusplus
}
#endif
