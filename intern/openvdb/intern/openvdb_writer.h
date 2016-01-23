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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENVDB_WRITER_H__
#define __OPENVDB_WRITER_H__

#include <openvdb/openvdb.h>

struct OpenVDBWriter {
private:
	openvdb::GridPtrVecPtr m_grids;
	openvdb::MetaMap::Ptr m_meta_map;

	int m_compression_flags;
	bool m_save_as_half;

public:
	OpenVDBWriter();
	~OpenVDBWriter();

	void insert(const openvdb::GridBase::Ptr &grid);
	void insert(const openvdb::GridBase &grid);

	void insertFloatMeta(const openvdb::Name &name, const float value);
	void insertIntMeta(const openvdb::Name &name, const int value);
	void insertVec3sMeta(const openvdb::Name &name, const openvdb::Vec3s &value);
	void insertVec3IMeta(const openvdb::Name &name, const openvdb::Vec3I &value);
	void insertMat4sMeta(const openvdb::Name &name, const float value[4][4]);

	void setFlags(const int compression, const bool save_as_half);

	void write(const openvdb::Name &filename) const;
};

#endif /* __OPENVDB_WRITER_H__ */
