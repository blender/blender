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
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_archive.h"

#include "BKE_blender_version.h"

#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_scene_types.h"

#ifdef WIN32
#  include "utfconv.h"
#endif

#include <fstream>

using Alembic::Abc::ErrorHandler;
using Alembic::Abc::kWrapExisting;
using Alembic::Abc::OArchive;

/* This kinda duplicates CreateArchiveWithInfo, but Alembic does not seem to
 * have a version supporting streams. */
static OArchive create_archive(std::ostream *ostream,
                               const std::string &scene_name,
                               double scene_fps)
{
  Alembic::Abc::MetaData abc_metadata;

  abc_metadata.set(Alembic::Abc::kApplicationNameKey, "Blender");
  abc_metadata.set(Alembic::Abc::kUserDescriptionKey, scene_name);
  abc_metadata.set("blender_version", std::string("v") + BKE_blender_version_string());
  abc_metadata.set("FramesPerTimeUnit", std::to_string(scene_fps));

  time_t raw_time;
  time(&raw_time);
  char buffer[128];

#if defined _WIN32 || defined _WIN64
  ctime_s(buffer, 128, &raw_time);
#else
  ctime_r(&raw_time, buffer);
#endif

  const std::size_t buffer_len = strlen(buffer);
  if (buffer_len > 0 && buffer[buffer_len - 1] == '\n') {
    buffer[buffer_len - 1] = '\0';
  }

  abc_metadata.set(Alembic::Abc::kDateWrittenKey, buffer);

  ErrorHandler::Policy policy = ErrorHandler::kThrowPolicy;
  Alembic::AbcCoreOgawa::WriteArchive archive_writer;
  return OArchive(archive_writer(ostream, abc_metadata), kWrapExisting, policy);
}

ArchiveWriter::ArchiveWriter(const char *filename,
                             const std::string &abc_scene_name,
                             const Scene *scene)
{
  /* Use stream to support unicode character paths on Windows. */
#ifdef WIN32
  UTF16_ENCODE(filename);
  std::wstring wstr(filename_16);
  m_outfile.open(wstr.c_str(), std::ios::out | std::ios::binary);
  UTF16_UN_ENCODE(filename);
#else
  m_outfile.open(filename, std::ios::out | std::ios::binary);
#endif

  m_archive = create_archive(&m_outfile, abc_scene_name, FPS);
}

OArchive &ArchiveWriter::archive()
{
  return m_archive;
}
