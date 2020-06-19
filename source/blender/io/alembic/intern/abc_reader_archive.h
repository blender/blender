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
#pragma once

/** \file
 * \ingroup balembic
 */

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>

#include <fstream>

struct Main;
struct Scene;

namespace blender {
namespace io {
namespace alembic {

/* Wrappers around input and output archives. The goal is to be able to use
 * streams so that unicode paths work on Windows (T49112), and to make sure that
 * the stream objects remain valid as long as the archives are open.
 */

class ArchiveReader {
  Alembic::Abc::IArchive m_archive;
  std::ifstream m_infile;
  std::vector<std::istream *> m_streams;

 public:
  ArchiveReader(struct Main *bmain, const char *filename);

  bool valid() const;

  Alembic::Abc::IObject getTop();
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
