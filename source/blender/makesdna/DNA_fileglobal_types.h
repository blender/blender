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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

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
  /** Commit timestamp from buildinfo. */
  uint64_t build_commit_timestamp;
  /** Hash from buildinfo. */
  char build_hash[16];
  /** File path where this was saved, for recover (1024 = FILE_MAX). */
  char filename[1024];
} FileGlobal;

/* minversion: in file, the oldest past blender version you can use compliant */
/* example: if in 2.43 the meshes lose mesh data, minversion is 2.43 then too */
/* or: in 2.42, subversion 1, same as above, minversion then is 2.42, min subversion 1 */
/* (defines for version are in the BKE_blender_version.h file, for historic reasons) */

#ifdef __cplusplus
}
#endif
