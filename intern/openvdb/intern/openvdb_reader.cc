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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 */

#include "openvdb_reader.h"
#include "openvdb_util.h"

OpenVDBReader::OpenVDBReader()
    : m_meta_map(new openvdb::MetaMap)
    , m_file(NULL)
{
	/* Although it is safe, it may not be good to have this here, could be done
	 * once instead of everytime we read a file. */
	openvdb::initialize();
}

OpenVDBReader::~OpenVDBReader()
{
	cleanupFile();
}

void OpenVDBReader::open(const openvdb::Name &filename)
{
	cleanupFile();

	try {
		m_file = new openvdb::io::File(filename);
		m_file->setCopyMaxBytes(0);
		m_file->open();

		m_meta_map = m_file->getMetadata();
	}
	/* Mostly to catch exceptions related to Blosc not being supported. */
	catch (const openvdb::IoError &e) {
		std::cerr << e.what() << '\n';
		cleanupFile();
	}
}

void OpenVDBReader::floatMeta(const openvdb::Name &name, float &value) const
{
	try {
		value = m_meta_map->metaValue<float>(name);
	}
	CATCH_KEYERROR;
}

void OpenVDBReader::intMeta(const openvdb::Name &name, int &value) const
{
	try {
		value = m_meta_map->metaValue<int>(name);
	}
	CATCH_KEYERROR;
}

void OpenVDBReader::vec3sMeta(const openvdb::Name &name, float value[3]) const
{
	try {
		openvdb::Vec3s meta_val = m_meta_map->metaValue<openvdb::Vec3s>(name);

		value[0] = meta_val.x();
		value[1] = meta_val.y();
		value[2] = meta_val.z();
	}
	CATCH_KEYERROR;
}

void OpenVDBReader::vec3IMeta(const openvdb::Name &name, int value[3]) const
{
	try {
		openvdb::Vec3i meta_val = m_meta_map->metaValue<openvdb::Vec3i>(name);

		value[0] = meta_val.x();
		value[1] = meta_val.y();
		value[2] = meta_val.z();
	}
	CATCH_KEYERROR;
}

void OpenVDBReader::mat4sMeta(const openvdb::Name &name, float value[4][4]) const
{
	try {
		openvdb::Mat4s meta_val = m_meta_map->metaValue<openvdb::Mat4s>(name);

		for (int i =  0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				value[i][j] = meta_val[i][j];
			}
		}
	}
	CATCH_KEYERROR;
}

bool OpenVDBReader::hasGrid(const openvdb::Name &name) const
{
	return m_file->hasGrid(name);
}

openvdb::GridBase::Ptr OpenVDBReader::getGrid(const openvdb::Name &name) const
{
	return m_file->readGrid(name);
}

size_t OpenVDBReader::numGrids() const
{
	return m_file->getGrids()->size();
}

void OpenVDBReader::cleanupFile()
{
	if (m_file) {
		m_file->close();
		delete m_file;
	}
}
