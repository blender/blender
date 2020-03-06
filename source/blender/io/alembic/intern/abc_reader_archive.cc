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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_archive.h"

extern "C" {
#include "BKE_main.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
}

#ifdef WIN32
#  include "utfconv.h"
#endif

#include <fstream>

using Alembic::Abc::ErrorHandler;
using Alembic::Abc::Exception;
using Alembic::Abc::IArchive;
using Alembic::Abc::kWrapExisting;

static IArchive open_archive(const std::string &filename,
                             const std::vector<std::istream *> &input_streams,
                             bool &is_hdf5)
{
  is_hdf5 = false;

  try {
    Alembic::AbcCoreOgawa::ReadArchive archive_reader(input_streams);

    return IArchive(archive_reader(filename), kWrapExisting, ErrorHandler::kThrowPolicy);
  }
  catch (const Exception &e) {
    std::cerr << e.what() << '\n';

#ifdef WITH_ALEMBIC_HDF5
    try {
      is_hdf5 = true;
      Alembic::AbcCoreAbstract::ReadArraySampleCachePtr cache_ptr;

      return IArchive(Alembic::AbcCoreHDF5::ReadArchive(),
                      filename.c_str(),
                      ErrorHandler::kThrowPolicy,
                      cache_ptr);
    }
    catch (const Exception &) {
      std::cerr << e.what() << '\n';
      return IArchive();
    }
#else
    /* Inspect the file to see whether it's really a HDF5 file. */
    char header[4]; /* char(0x89) + "HDF" */
    std::ifstream the_file(filename.c_str(), std::ios::in | std::ios::binary);
    if (!the_file) {
      std::cerr << "Unable to open " << filename << std::endl;
    }
    else if (!the_file.read(header, sizeof(header))) {
      std::cerr << "Unable to read from " << filename << std::endl;
    }
    else if (strncmp(header + 1, "HDF", 3)) {
      std::cerr << filename << " has an unknown file format, unable to read." << std::endl;
    }
    else {
      is_hdf5 = true;
      std::cerr << filename << " is in the obsolete HDF5 format, unable to read." << std::endl;
    }

    if (the_file.is_open()) {
      the_file.close();
    }

    return IArchive();
#endif
  }

  return IArchive();
}

ArchiveReader::ArchiveReader(struct Main *bmain, const char *filename)
{
  char abs_filename[FILE_MAX];
  BLI_strncpy(abs_filename, filename, FILE_MAX);
  BLI_path_abs(abs_filename, BKE_main_blendfile_path(bmain));

#ifdef WIN32
  UTF16_ENCODE(abs_filename);
  std::wstring wstr(abs_filename_16);
  m_infile.open(wstr.c_str(), std::ios::in | std::ios::binary);
  UTF16_UN_ENCODE(abs_filename);
#else
  m_infile.open(abs_filename, std::ios::in | std::ios::binary);
#endif

  m_streams.push_back(&m_infile);

  m_archive = open_archive(abs_filename, m_streams, m_is_hdf5);

  /* We can't open an HDF5 file from a stream, so close it. */
  if (m_is_hdf5) {
    m_infile.close();
    m_streams.clear();
  }
}

bool ArchiveReader::is_hdf5() const
{
  return m_is_hdf5;
}

bool ArchiveReader::valid() const
{
  return m_archive.valid();
}

Alembic::Abc::IObject ArchiveReader::getTop()
{
  return m_archive.getTop();
}
