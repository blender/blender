/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 *
 * Private function prototypes for bmesh public API.
 * This file is a grab-bag of functions from various
 * parts of the bmesh internals.
 */

typedef enum {
  IS_OK = 0,
  IS_NULL = (1 << 0),
  IS_WRONG_TYPE = (1 << 1),

  IS_VERT_WRONG_EDGE_TYPE = (1 << 2),

  IS_EDGE_NULL_DISK_LINK = (1 << 3),
  IS_EDGE_WRONG_LOOP_TYPE = (1 << 4),
  IS_EDGE_WRONG_FACE_TYPE = (1 << 5),
  IS_EDGE_NULL_RADIAL_LINK = (1 << 6),
  IS_EDGE_ZERO_FACE_LENGTH = (1 << 7),

  IS_LOOP_WRONG_FACE_TYPE = (1 << 8),
  IS_LOOP_WRONG_EDGE_TYPE = (1 << 9),
  IS_LOOP_WRONG_VERT_TYPE = (1 << 10),
  IS_LOOP_VERT_NOT_IN_EDGE = (1 << 11),
  IS_LOOP_NULL_CYCLE_LINK = (1 << 12),
  IS_LOOP_ZERO_FACE_LENGTH = (1 << 13),
  IS_LOOP_WRONG_FACE_LENGTH = (1 << 14),
  IS_LOOP_WRONG_RADIAL_LENGTH = (1 << 15),

  IS_FACE_NULL_LOOP = (1 << 16),
  IS_FACE_WRONG_LOOP_FACE = (1 << 17),
  IS_FACE_NULL_EDGE = (1 << 18),
  IS_FACE_NULL_VERT = (1 << 19),
  IS_FACE_LOOP_VERT_NOT_IN_EDGE = (1 << 20),
  IS_FACE_LOOP_WRONG_RADIAL_LENGTH = (1 << 21),
  IS_FACE_LOOP_WRONG_DISK_LENGTH = (1 << 22),
  IS_FACE_LOOP_DUPE_LOOP = (1 << 23),
  IS_FACE_LOOP_DUPE_VERT = (1 << 24),
  IS_FACE_LOOP_DUPE_EDGE = (1 << 25),
  IS_FACE_WRONG_LENGTH = (1 << 26),
} BMeshInternalError;

#ifdef __cplusplus
extern "C" {
#endif

/* returns positive nonzero on error */

#ifdef NDEBUG
/* No error checking for release,
 * it can take most of the CPU time when running some tools. */
#  define BM_CHECK_ELEMENT(el) (void)(el)
#else
/**
 * Check the element is valid.
 *
 * BMESH_TODO, when this raises an error the output is incredibly confusing.
 * need to have some nice way to print/debug what the heck's going on.
 */
int bmesh_elem_check(void *element, char htype);
#  define BM_CHECK_ELEMENT(el) \
    { \
      if (bmesh_elem_check(el, ((BMHeader *)el)->htype)) { \
        printf( \
            "check_element failure, with code %i on line %i in file\n" \
            "    \"%s\"\n\n", \
            bmesh_elem_check(el, ((BMHeader *)el)->htype), \
            __LINE__, \
            __FILE__); \
      } \
    } \
    ((void)0)
#endif

int bmesh_radial_length(const BMLoop *l);
int bmesh_disk_count_at_most(const BMVert *v, int count_max);
int bmesh_disk_count(const BMVert *v);

/**
 * Internal BMHeader.api_flag
 * \note Ensure different parts of the API do not conflict
 * on using these internal flags!
 */
enum {
  _FLAG_JF = (1 << 0),       /* Join faces. */
  _FLAG_MF = (1 << 1),       /* Make face. */
  _FLAG_MV = (1 << 1),       /* Make face, vertex. */
  _FLAG_OVERLAP = (1 << 2),  /* General overlap flag. */
  _FLAG_WALK = (1 << 3),     /* General walk flag (keep clean). */
  _FLAG_WALK_ALT = (1 << 4), /* Same as #_FLAG_WALK, for when a second tag is needed. */

  _FLAG_ELEM_CHECK = (1 << 7), /* Reserved for bmesh_elem_check. */
};

#define BM_ELEM_API_FLAG_ENABLE(element, f) \
  { \
    ((element)->head.api_flag |= (f)); \
  } \
  (void)0
#define BM_ELEM_API_FLAG_DISABLE(element, f) \
  { \
    ((element)->head.api_flag &= (uchar) ~(f)); \
  } \
  (void)0
#define BM_ELEM_API_FLAG_TEST(element, f) ((element)->head.api_flag & (f))
#define BM_ELEM_API_FLAG_CLEAR(element) \
  { \
    ((element)->head.api_flag = 0); \
  } \
  (void)0

/**
 * \brief POLY ROTATE PLANE
 *
 * Rotates a polygon so that its
 * normal is pointing towards the mesh Z axis
 */
void poly_rotate_plane(const float normal[3], float (*verts)[3], uint nverts);

void bm_kill_only_face(BMesh *bm, BMFace *f);
void bm_kill_only_loop(BMesh *bm, BMLoop *l);
const char *bm_get_error_str(int err);
int bmesh_elem_check(void *element, const char htype);

/* include the rest of our private declarations */
#include "bmesh_structure.h"

#ifdef __cplusplus
}
#endif
