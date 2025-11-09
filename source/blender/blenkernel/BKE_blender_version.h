/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/** \file
 * \ingroup bke
 */

/**
 * The lines below use regex from scripts to extract their values,
 * Keep this in mind when modifying this file and keep this comment above the defines.
 *
 * \note Use #STRINGIFY() rather than defining with quotes.
 */

/** Blender major and minor version. */
#define BLENDER_VERSION 501
/** Blender patch version for bug-fix releases. */
#define BLENDER_VERSION_PATCH 0
/** Blender release cycle stage: alpha/beta/rc/release. */
#define BLENDER_VERSION_CYCLE alpha
/** Blender release type suffix. LTS or blank. */
#define BLENDER_VERSION_SUFFIX

/* Blender file format version. */
#define BLENDER_FILE_VERSION BLENDER_VERSION
#define BLENDER_FILE_SUBVERSION 4

/* Minimum Blender version that supports reading file written with the current
 * version. Older Blender versions will test this and cancel loading the file, showing a warning to
 * the user.
 *
 * See
 * https://developer.blender.org/docs/handbook/guidelines/compatibility_handling_for_blend_files/
 * for details. */
#define BLENDER_FILE_MIN_VERSION 405
#define BLENDER_FILE_MIN_SUBVERSION 85

/** User readable version string. */
const char *BKE_blender_version_string(void);

/** As above but does not show patch version. */
const char *BKE_blender_version_string_compact(void);

/** Returns true when version cycle is alpha, otherwise (beta, rc) returns false. */
bool BKE_blender_version_is_alpha(void);

/** Returns true when version suffix is LTS, otherwise returns false. */
bool BKE_blender_version_is_lts(void);

/**
 * Fill in given string buffer with user-readable formatted file version and subversion (if
 * provided).
 *
 * \param str_buff: a char buffer where the formatted string is written,
 * minimal recommended size is 8, or 16 if subversion is provided.
 *
 * \param file_subversion: the file subversion, if given value < 0, it is ignored, and only the
 * `file_version` is used.
 */
void BKE_blender_version_blendfile_string_from_values(char *str_buff,
                                                      const size_t str_buff_maxncpy,
                                                      const short file_version,
                                                      const short file_subversion);
