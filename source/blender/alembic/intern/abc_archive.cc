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

#include "abc_archive.h"
extern "C"
{
	#include "BKE_blender_version.h"
}

#ifdef WIN32
#  include "utfconv.h"
#endif

#include <fstream>

using Alembic::Abc::Exception;
using Alembic::Abc::ErrorHandler;
using Alembic::Abc::IArchive;
using Alembic::Abc::kWrapExisting;
using Alembic::Abc::OArchive;

static IArchive open_archive(const std::string &filename,
                             const std::vector<std::istream *> &input_streams,
                             bool &is_hdf5)
{
	is_hdf5 = false;

	try {
		Alembic::AbcCoreOgawa::ReadArchive archive_reader(input_streams);

		return IArchive(archive_reader(filename),
		                kWrapExisting,
		                ErrorHandler::kThrowPolicy);
	}
	catch (const Exception &e) {
		std::cerr << e.what() << '\n';

#ifdef WITH_ALEMBIC_HDF5
		try {
			is_hdf5 = true;
			Alembic::AbcCoreAbstract::ReadArraySampleCachePtr cache_ptr;

			return IArchive(Alembic::AbcCoreHDF5::ReadArchive(),
			                filename.c_str(), ErrorHandler::kThrowPolicy,
			                cache_ptr);
		}
		catch (const Exception &) {
			std::cerr << e.what() << '\n';
			return IArchive();
		}
#else
		/* Inspect the file to see whether it's really a HDF5 file. */
		char header[4];  /* char(0x89) + "HDF" */
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

ArchiveReader::ArchiveReader(const char *filename)
{
#ifdef WIN32
	UTF16_ENCODE(filename);
	std::wstring wstr(filename_16);
	m_infile.open(wstr.c_str(), std::ios::in | std::ios::binary);
	UTF16_UN_ENCODE(filename);
#else
	m_infile.open(filename, std::ios::in | std::ios::binary);
#endif

	m_streams.push_back(&m_infile);

	m_archive = open_archive(filename, m_streams, m_is_hdf5);

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

/* ************************************************************************** */

/* This kinda duplicates CreateArchiveWithInfo, but Alembic does not seem to
 * have a version supporting streams. */
static OArchive create_archive(std::ostream *ostream,
                               const std::string &filename,
                               const std::string &scene_name,
                               Alembic::Abc::MetaData &md,
                               bool ogawa)
{
	md.set(Alembic::Abc::kApplicationNameKey, "Blender");
	md.set(Alembic::Abc::kUserDescriptionKey, scene_name);
	md.set("blender_version", versionstr);

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

	md.set(Alembic::Abc::kDateWrittenKey, buffer);

	ErrorHandler::Policy policy = ErrorHandler::kThrowPolicy;

#ifdef WITH_ALEMBIC_HDF5
	if (!ogawa) {
		return OArchive(Alembic::AbcCoreHDF5::WriteArchive(), filename, md, policy);
	}
#else
	static_cast<void>(filename);
	static_cast<void>(ogawa);
#endif

	Alembic::AbcCoreOgawa::WriteArchive archive_writer;
	return OArchive(archive_writer(ostream, md), kWrapExisting, policy);
}

ArchiveWriter::ArchiveWriter(const char *filename, const char *scene, bool do_ogawa, Alembic::Abc::MetaData &md)
{
	/* Use stream to support unicode character paths on Windows. */
	if (do_ogawa) {
#ifdef WIN32
		UTF16_ENCODE(filename);
		std::wstring wstr(filename_16);
		m_outfile.open(wstr.c_str(), std::ios::out | std::ios::binary);
		UTF16_UN_ENCODE(filename);
#else
		m_outfile.open(filename, std::ios::out | std::ios::binary);
#endif
	}

	m_archive = create_archive(&m_outfile,
	                           filename,
	                           scene,
	                           md,
	                           do_ogawa);
}

OArchive &ArchiveWriter::archive()
{
	return m_archive;
}
