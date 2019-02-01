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

#ifndef __OPENVDB_READER_H__
#define __OPENVDB_READER_H__

#include <openvdb/openvdb.h>

struct OpenVDBReader {
private:
	openvdb::MetaMap::Ptr m_meta_map;
	openvdb::io::File *m_file;

	void cleanupFile();

public:
	OpenVDBReader();
	~OpenVDBReader();

	void open(const openvdb::Name &filename);

	void floatMeta(const openvdb::Name &name, float &value) const;
	void intMeta(const openvdb::Name &name, int &value) const;
	void vec3sMeta(const openvdb::Name &name, float value[3]) const;
	void vec3IMeta(const openvdb::Name &name, int value[3]) const;
	void mat4sMeta(const openvdb::Name &name, float value[4][4]) const;

	bool hasGrid(const openvdb::Name &name) const;
	openvdb::GridBase::Ptr getGrid(const openvdb::Name &name) const;
	size_t numGrids() const;
};

#endif /* __OPENVDB_READER_H__ */
