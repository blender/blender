/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include <Alembic/Abc/IArchive.h>
#include <Alembic/Abc/IObject.h>

#include <fstream>
#include <vector>

namespace blender {

struct Main;

namespace io::alembic {

/* Represents the time range in seconds for animated data inside of an Alembic archive. The time
 * range is [min, max]. */
struct TimeInfo {
  Alembic::Abc::chrono_t min_time = std::numeric_limits<Alembic::Abc::chrono_t>::max();
  Alembic::Abc::chrono_t max_time = -std::numeric_limits<Alembic::Abc::chrono_t>::max();

  bool is_valid() const
  {
    return min_time <= max_time &&
           min_time != std::numeric_limits<Alembic::Abc::chrono_t>::max() &&
           max_time != -std::numeric_limits<Alembic::Abc::chrono_t>::max();
  }
};

/**
 * Wrappers around input and output archives. The goal is to be able to use
 * streams so that unicode paths work on Windows (#49112), and to make sure that
 * the stream objects remain valid as long as the archives are open.
 */
class ArchiveReader {
  Alembic::Abc::IArchive m_archive;
  std::ifstream m_infile;
  std::vector<std::istream *> m_streams;

  std::vector<ArchiveReader *> m_readers;

  ArchiveReader(const std::vector<ArchiveReader *> &readers);

  ArchiveReader(const struct Main *bmain, const char *filename);

 public:
  static ArchiveReader *get(const struct Main *bmain, const std::vector<const char *> &filenames);

  ~ArchiveReader();

  bool valid() const;

  Alembic::Abc::IObject getTop();

  /* Detect if the Archive was written by Blender prior to 4.4. */
  bool is_blender_archive_version_prior_44();

  TimeInfo getTimeInfo();
};

}  // namespace io::alembic
}  // namespace blender
