/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_archive.h"

#include "Alembic/AbcCoreLayer/Read.h"

#include "BKE_main.h"

#include "BLI_path_util.h"
#include "BLI_string.h"

#ifdef WIN32
#  include "utfconv.h"
#endif

#include <fstream>

using Alembic::Abc::ErrorHandler;
using Alembic::Abc::Exception;
using Alembic::Abc::IArchive;
using Alembic::Abc::kWrapExisting;

namespace blender::io::alembic {

static IArchive open_archive(const std::string &filename,
                             const std::vector<std::istream *> &input_streams)
{
  try {
    Alembic::AbcCoreOgawa::ReadArchive archive_reader(input_streams);

    return IArchive(archive_reader(filename), kWrapExisting, ErrorHandler::kThrowPolicy);
  }
  catch (const Exception &e) {
    std::cerr << e.what() << '\n';

    /* Inspect the file to see whether it's actually a HDF5 file. */
    char header[4]; /* char(0x89) + "HDF" */
    std::ifstream the_file(filename.c_str(), std::ios::in | std::ios::binary);
    if (!the_file) {
      std::cerr << "Unable to open " << filename << std::endl;
    }
    else if (!the_file.read(header, sizeof(header))) {
      std::cerr << "Unable to read from " << filename << std::endl;
    }
    else if (strncmp(header + 1, "HDF", 3) != 0) {
      std::cerr << filename << " has an unknown file format, unable to read." << std::endl;
    }
    else {
      std::cerr << filename << " is in the obsolete HDF5 format, unable to read." << std::endl;
    }

    if (the_file.is_open()) {
      the_file.close();
    }
  }

  return IArchive();
}

ArchiveReader *ArchiveReader::get(struct Main *bmain, const std::vector<const char *> &filenames)
{
  std::vector<ArchiveReader *> readers;

  for (const char *filename : filenames) {
    ArchiveReader *reader = new ArchiveReader(bmain, filename);

    if (!reader->valid()) {
      delete reader;
      continue;
    }

    readers.push_back(reader);
  }

  if (readers.empty()) {
    return nullptr;
  }

  if (readers.size() == 1) {
    return readers[0];
  }

  return new ArchiveReader(readers);
}

ArchiveReader::ArchiveReader(const std::vector<ArchiveReader *> &readers) : m_readers(readers)
{
  Alembic::AbcCoreLayer::ArchiveReaderPtrs archives;

  for (ArchiveReader *reader : readers) {
    archives.push_back(reader->m_archive.getPtr());
  }

  Alembic::AbcCoreLayer::ReadArchive layer;
  Alembic::AbcCoreAbstract::ArchiveReaderPtr arPtr = layer(archives);

  m_archive = IArchive(arPtr, kWrapExisting, ErrorHandler::kThrowPolicy);
}

ArchiveReader::ArchiveReader(struct Main *bmain, const char *filename)
{
  char abs_filepath[FILE_MAX];
  STRNCPY(abs_filepath, filename);
  BLI_path_abs(abs_filepath, BKE_main_blendfile_path(bmain));

#ifdef WIN32
  UTF16_ENCODE(abs_filepath);
  std::wstring wstr(abs_filepath_16);
  m_infile.open(wstr.c_str(), std::ios::in | std::ios::binary);
  UTF16_UN_ENCODE(abs_filepath);
#else
  m_infile.open(abs_filepath, std::ios::in | std::ios::binary);
#endif

  m_streams.push_back(&m_infile);

  m_archive = open_archive(abs_filepath, m_streams);
}

ArchiveReader::~ArchiveReader()
{
  for (ArchiveReader *reader : m_readers) {
    delete reader;
  }
}

bool ArchiveReader::valid() const
{
  return m_archive.valid();
}

Alembic::Abc::IObject ArchiveReader::getTop()
{
  return m_archive.getTop();
}

}  // namespace blender::io::alembic
