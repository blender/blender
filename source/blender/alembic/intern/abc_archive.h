/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef __ABC_ARCHIVE_H__
#define __ABC_ARCHIVE_H__

#include <Alembic/Abc/All.h>

#ifdef WITH_ALEMBIC_HDF5
#  include <Alembic/AbcCoreHDF5/All.h>
#endif

#include <Alembic/AbcCoreOgawa/All.h>

#include <fstream>

/* Wrappers around input and output archives. The goal is to be able to use
 * streams so that unicode paths work on Windows (T49112), and to make sure that
 * the stream objects remain valid as long as the archives are open.
 */

class ArchiveReader {
	Alembic::Abc::IArchive m_archive;
	std::ifstream m_infile;
	std::vector<std::istream *> m_streams;
	bool m_is_hdf5;

public:
	explicit ArchiveReader(const char *filename);

	bool valid() const;

	/**
	 * Returns true when either Blender is compiled with HDF5 support and
	 * the archive was successfully opened (valid() will also return true),
	 * or when Blender was built without HDF5 support but a HDF5 file was
	 * detected (valid() will return false).
	 */
	bool is_hdf5() const;

	Alembic::Abc::IObject getTop();
};

class ArchiveWriter {
	std::ofstream m_outfile;
	Alembic::Abc::OArchive m_archive;

public:
	explicit ArchiveWriter(const char *filename, const char *scene, bool do_ogawa, Alembic::Abc::MetaData &md);

	Alembic::Abc::OArchive &archive();
};

#endif /* __ABC_ARCHIVE_H__ */
