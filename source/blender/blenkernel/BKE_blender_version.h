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
#ifndef __BKE_BLENDER_VERSION_H__
#define __BKE_BLENDER_VERSION_H__

/** \file
 * \ingroup bke
 */

/**
 * The lines below use regex from scripts to extract their values,
 * Keep this in mind when modifying this file and keep this comment above the defines.
 *
 * \note Use #STRINGIFY() rather than defining with quotes.
 */

/* Blender major and minor version. */
#define BLENDER_VERSION 283
/* Blender patch version for bugfix releases. */
#define BLENDER_VERSION_PATCH 0
/** Blender release cycle stage: alpha/beta/rc/release. */
#define BLENDER_VERSION_CYCLE release 

/* Blender file format version. */
#define BLENDER_FILE_VERSION BLENDER_VERSION
#define BLENDER_FILE_SUBVERSION 18

/* Minimum Blender version that supports reading file written with the current
 * version. Older Blender versions will test this and show a warning if the file
 * was written with too new a version. */
#define BLENDER_FILE_MIN_VERSION 280
#define BLENDER_FILE_MIN_SUBVERSION 0

/** User readable version string. */
const char *BKE_blender_version_string(void);

#endif /* __BKE_BLENDER_VERSION_H__ */
