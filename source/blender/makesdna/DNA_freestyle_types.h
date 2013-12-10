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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __DNA_FREESTYLE_TYPES_H__
#define __DNA_FREESTYLE_TYPES_H__

/** \file DNA_freestyle_types.h
 *  \ingroup DNA
 */

#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FreestyleLineStyle;
struct Group;
struct Text;

/* FreestyleConfig::flags */
#define FREESTYLE_SUGGESTIVE_CONTOURS_FLAG  (1 << 0)
#define FREESTYLE_RIDGES_AND_VALLEYS_FLAG   (1 << 1)
#define FREESTYLE_MATERIAL_BOUNDARIES_FLAG  (1 << 2)
#define FREESTYLE_FACE_SMOOTHNESS_FLAG      (1 << 3)
#define FREESTYLE_ADVANCED_OPTIONS_FLAG     (1 << 4)
#define FREESTYLE_CULLING                   (1 << 5)

/* FreestyleConfig::mode */
#define FREESTYLE_CONTROL_SCRIPT_MODE  1
#define FREESTYLE_CONTROL_EDITOR_MODE  2

/* FreestyleLineSet::flags */
#define FREESTYLE_LINESET_CURRENT  (1 << 0)
#define FREESTYLE_LINESET_ENABLED  (1 << 1)
#define FREESTYLE_LINESET_FE_NOT   (1 << 2)
#define FREESTYLE_LINESET_FE_AND   (1 << 3)
#define FREESTYLE_LINESET_GR_NOT   (1 << 4)
#define FREESTYLE_LINESET_FM_NOT   (1 << 5)
#define FREESTYLE_LINESET_FM_BOTH  (1 << 6)

/* FreestyleLineSet::selection */
#define FREESTYLE_SEL_VISIBILITY    (1 << 0)
#define FREESTYLE_SEL_EDGE_TYPES    (1 << 1)
#define FREESTYLE_SEL_GROUP         (1 << 2)
#define FREESTYLE_SEL_IMAGE_BORDER  (1 << 3)
#define FREESTYLE_SEL_FACE_MARK     (1 << 4)

/* FreestyleLineSet::edge_types, exclude_edge_types */
#define FREESTYLE_FE_SILHOUETTE          (1 << 0)
#define FREESTYLE_FE_BORDER              (1 << 1)
#define FREESTYLE_FE_CREASE              (1 << 2)
#define FREESTYLE_FE_RIDGE_VALLEY        (1 << 3)
/* Note: FREESTYLE_FE_VALLEY = (1 << 4) is no longer used */
#define FREESTYLE_FE_SUGGESTIVE_CONTOUR  (1 << 5)
#define FREESTYLE_FE_MATERIAL_BOUNDARY   (1 << 6)
#define FREESTYLE_FE_CONTOUR             (1 << 7)
#define FREESTYLE_FE_EXTERNAL_CONTOUR    (1 << 8)
#define FREESTYLE_FE_EDGE_MARK           (1 << 9)

/* FreestyleLineSet::qi */
#define FREESTYLE_QI_VISIBLE  1
#define FREESTYLE_QI_HIDDEN   2
#define FREESTYLE_QI_RANGE    3

/* FreestyleConfig::raycasting_algorithm */
/* Defines should be replaced with ViewMapBuilder::visibility_algo */
#define FREESTYLE_ALGO_REGULAR                      1
#define FREESTYLE_ALGO_FAST                         2
#define FREESTYLE_ALGO_VERYFAST                     3
#define FREESTYLE_ALGO_CULLED_ADAPTIVE_TRADITIONAL  4
#define FREESTYLE_ALGO_ADAPTIVE_TRADITIONAL         5
#define FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE   6
#define FREESTYLE_ALGO_ADAPTIVE_CUMULATIVE          7

typedef struct FreestyleLineSet {
	struct FreestyleLineSet *next, *prev;

	char name[64]; /* line set name, MAX_NAME */
	int flags;

	int selection; /* selection criteria */
	short qi; /* quantitative invisibility */
	short pad1;
	int qi_start, qi_end;
	int edge_types, exclude_edge_types; /* feature edge types */
	int pad2;
	struct Group *group; /* group of target objects */

	struct FreestyleLineStyle *linestyle;
} FreestyleLineSet;

typedef struct FreestyleModuleConfig {
	struct FreestyleModuleConfig *next, *prev;

	struct Text *script;
	short is_displayed;
	short pad[3];
} FreestyleModuleConfig;

typedef struct FreestyleConfig {
	ListBase modules;

	int mode; /* scripting, editor */
	int raycasting_algorithm  DNA_DEPRECATED;
	int flags; /* suggestive contours, ridges/valleys, material boundaries */
	float sphere_radius;
	float dkr_epsilon;
	float crease_angle; /* in radians! */

	ListBase linesets;
} FreestyleConfig;

#ifdef __cplusplus
}
#endif

#endif /* __DNA_FREESTYLE_TYPES_H__ */
