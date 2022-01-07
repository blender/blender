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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bmesh
 *
 * Private function prototypes for bmesh public API.
 * This file is a grab-bag of functions from various
 * parts of the bmesh internals.
 */

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

/* include the rest of our private declarations */
#include "bmesh_structure.h"
