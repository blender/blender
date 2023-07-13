/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
#define BLENDER_VERSION 400
/* Blender patch version for bug-fix releases. */
#define BLENDER_VERSION_PATCH 0
/** Blender release cycle stage: alpha/beta/rc/release. */
#define BLENDER_VERSION_CYCLE alpha

/* Blender file format version. */
#define BLENDER_FILE_VERSION BLENDER_VERSION
#define BLENDER_FILE_SUBVERSION 10

/* Minimum Blender version that supports reading file written with the current
 * version. Older Blender versions will test this and show a warning if the file
 * was written with too new a version. */
#define BLENDER_FILE_MIN_VERSION 400
#define BLENDER_FILE_MIN_SUBVERSION 3

/** User readable version string. */
const char *BKE_blender_version_string(void);

/* Returns true when version cycle is alpha, otherwise (beta, rc) returns false. */
bool BKE_blender_version_is_alpha(void);

#ifdef __cplusplus
}
#endif
