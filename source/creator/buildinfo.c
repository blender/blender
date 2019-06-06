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
 * \ingroup creator
 */

#ifdef WITH_BUILDINFO_HEADER
#  include "buildinfo.h"
#endif

#ifdef BUILD_DATE

extern char build_date[];
extern char build_time[];
extern char build_hash[];
extern unsigned long build_commit_timestamp;
extern char build_commit_date[];
extern char build_commit_time[];
extern char build_branch[];
extern char build_platform[];
extern char build_type[];
extern char build_cflags[];
extern char build_cxxflags[];
extern char build_linkflags[];
extern char build_system[];

/* Currently only these are defined in the header. */
char build_date[] = BUILD_DATE;
char build_time[] = BUILD_TIME;
char build_hash[] = BUILD_HASH;
unsigned long build_commit_timestamp = BUILD_COMMIT_TIMESTAMP;
char build_commit_date[16] = "\0";
char build_commit_time[16] = "\0";
char build_branch[] = BUILD_BRANCH;

char build_platform[] = BUILD_PLATFORM;
char build_type[] = BUILD_TYPE;

#  ifdef BUILD_CFLAGS
char build_cflags[] = BUILD_CFLAGS;
char build_cxxflags[] = BUILD_CXXFLAGS;
char build_linkflags[] = BUILD_LINKFLAGS;
char build_system[] = BUILD_SYSTEM;
#  else
char build_cflags[] = "unmaintained buildsystem alert!";
char build_cxxflags[] = "unmaintained buildsystem alert!";
char build_linkflags[] = "unmaintained buildsystem alert!";
char build_system[] = "unmaintained buildsystem alert!";
#  endif

#endif  // BUILD_DATE
