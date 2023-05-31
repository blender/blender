/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * FileGlobal stores a part of the current user-interface settings at
 * the moment of saving, and the file-specific settings.
 */
typedef struct FileGlobal {
  /** Needs to be here, for human file-format recognition (keep first!). */
  char subvstr[4];

  short subversion;
  short minversion, minsubversion;
  char _pad[6];
  struct bScreen *curscreen;
  struct Scene *curscene;
  struct ViewLayer *cur_view_layer;
  void *_pad1;

  int fileflags;
  int globalf;
  /** Commit timestamp from `buildinfo`. */
  uint64_t build_commit_timestamp;
  /** Hash from `buildinfo`. */
  char build_hash[16];
  /** File path where this was saved, for recover (1024 = FILE_MAX). */
  char filepath[1024];
} FileGlobal;

/* minversion: in file, the oldest past blender version you can use compliant */
/* example: if in 2.43 the meshes lose mesh data, minversion is 2.43 then too */
/* or: in 2.42, subversion 1, same as above, minversion then is 2.42, min subversion 1 */
/* (defines for version are in the BKE_blender_version.h file, for historic reasons) */

#ifdef __cplusplus
}
#endif
