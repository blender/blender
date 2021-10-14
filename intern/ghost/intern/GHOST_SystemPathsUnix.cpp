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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 */

#include <cstdio>
#include <sstream>

#include "GHOST_SystemPathsUnix.h"

#include "GHOST_Debug.h"

// For timing

#include <sys/time.h>
#include <unistd.h>

#include <cstdlib> /* for exit */
#include <stdio.h> /* for fprintf only */

#include <pwd.h> /* for get home without use getenv() */
#include <string>

using std::string;

#ifdef PREFIX
static const char *static_path = PREFIX "/share";
#else
static const char *static_path = NULL;
#endif

GHOST_SystemPathsUnix::GHOST_SystemPathsUnix()
{
}

GHOST_SystemPathsUnix::~GHOST_SystemPathsUnix()
{
}

const char *GHOST_SystemPathsUnix::getSystemDir(int, const char *versionstr) const
{
  /* no prefix assumes a portable build which only uses bundled scripts */
  if (static_path) {
    static string system_path = string(static_path) + "/blender/" + versionstr;
    return system_path.c_str();
  }

  return NULL;
}

const char *GHOST_SystemPathsUnix::getUserDir(int version, const char *versionstr) const
{
  static string user_path = "";
  static int last_version = 0;

  /* in blender 2.64, we migrate to XDG. to ensure the copy previous settings
   * operator works we give a different path depending on the requested version */
  if (version < 264) {
    if (user_path.empty() || last_version != version) {
      const char *home = getenv("HOME");

      last_version = version;

      if (home) {
        user_path = string(home) + "/.blender/" + versionstr;
      }
      else {
        return NULL;
      }
    }
    return user_path.c_str();
  }
  else {
    if (user_path.empty() || last_version != version) {
      const char *home = getenv("XDG_CONFIG_HOME");

      last_version = version;

      if (home) {
        user_path = string(home) + "/blender/" + versionstr;
      }
      else {
        home = getenv("HOME");

        if (home == NULL)
          home = getpwuid(getuid())->pw_dir;

        user_path = string(home) + "/.config/blender/" + versionstr;
      }
    }

    return user_path.c_str();
  }
}

const char *GHOST_SystemPathsUnix::getUserSpecialDir(GHOST_TUserSpecialDirTypes type) const
{
  const char *type_str;
  std::string add_path = "";

  switch (type) {
    case GHOST_kUserSpecialDirDesktop:
      type_str = "DESKTOP";
      break;
    case GHOST_kUserSpecialDirDocuments:
      type_str = "DOCUMENTS";
      break;
    case GHOST_kUserSpecialDirDownloads:
      type_str = "DOWNLOAD";
      break;
    case GHOST_kUserSpecialDirMusic:
      type_str = "MUSIC";
      break;
    case GHOST_kUserSpecialDirPictures:
      type_str = "PICTURES";
      break;
    case GHOST_kUserSpecialDirVideos:
      type_str = "VIDEOS";
      break;
    case GHOST_kUserSpecialDirCaches: {
      const char *cache_dir = getenv("XDG_CACHE_HOME");
      if (cache_dir) {
        return cache_dir;
      }
      /* Fallback to ~home/.cache/.
       * When invoking `xdg-user-dir` without parameters the user folder
       * will be read. `.cache` will be appended. */
      type_str = "";
      add_path = ".cache";
      break;
    }
    default:
      GHOST_ASSERT(
          false,
          "GHOST_SystemPathsUnix::getUserSpecialDir(): Invalid enum value for type parameter");
      return NULL;
  }

  static string path = "";
  /* Pipe `stderr` to `/dev/null` to avoid error prints. We will fail gracefully still. */
  string command = string("xdg-user-dir ") + type_str + " 2> /dev/null";

  FILE *fstream = popen(command.c_str(), "r");
  if (fstream == NULL) {
    return NULL;
  }
  std::stringstream path_stream;
  while (!feof(fstream)) {
    char c = fgetc(fstream);
    /* `xdg-user-dir` ends the path with '\n'. */
    if (c == '\n') {
      break;
    }
    path_stream << c;
  }
  if (pclose(fstream) == -1) {
    perror("GHOST_SystemPathsUnix::getUserSpecialDir failed at pclose()");
    return NULL;
  }

  if (!add_path.empty()) {
    path_stream << '/' << add_path;
  }

  path = path_stream.str();
  return path[0] ? path.c_str() : NULL;
}

const char *GHOST_SystemPathsUnix::getBinaryDir() const
{
  return NULL;
}

void GHOST_SystemPathsUnix::addToSystemRecentFiles(const char * /*filename*/) const
{
  /* TODO: implement for X11 */
}
