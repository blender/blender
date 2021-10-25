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

#ifndef __ABC_CUSTOMDATA_H__
#define __ABC_CUSTOMDATA_H__

#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>

struct CustomData;
struct MLoop;
struct MLoopUV;
struct MPoly;
struct MVert;

using Alembic::Abc::ICompoundProperty;
using Alembic::Abc::OCompoundProperty;

struct UVSample {
	std::vector<Imath::V2f> uvs;
	std::vector<uint32_t> indices;
};

struct CDStreamConfig {
	MLoop *mloop;
	int totloop;

	MPoly *mpoly;
	int totpoly;

	MVert *mvert;
	int totvert;

	MLoopUV *mloopuv;

	CustomData *loopdata;

	bool pack_uvs;

	/* TODO(kevin): might need a better way to handle adding and/or updating
	 * custom datas such that it updates the custom data holder and its pointers
	 * properly. */
	void *user_data;
	void *(*add_customdata_cb)(void *user_data, const char *name, int data_type);

	float weight;
	float time;
	Alembic::AbcGeom::index_t index;
	Alembic::AbcGeom::index_t ceil_index;

	CDStreamConfig()
	    : mloop(NULL)
	    , totloop(0)
	    , mpoly(NULL)
	    , totpoly(0)
	    , totvert(0)
	    , pack_uvs(false)
	    , user_data(NULL)
	    , add_customdata_cb(NULL)
	    , weight(0.0f)
	    , time(0.0f)
	    , index(0)
	    , ceil_index(0)
	{}
};

/* Get the UVs for the main UV property on a OSchema.
 * Returns the name of the UV layer.
 *
 * For now the active layer is used, maybe needs a better way to choose this. */
const char *get_uv_sample(UVSample &sample, const CDStreamConfig &config, CustomData *data);

void write_custom_data(const OCompoundProperty &prop,
                       const CDStreamConfig &config,
                       CustomData *data,
                       int data_type);

void read_custom_data(const std::string & iobject_full_name,
                      const ICompoundProperty &prop,
                      const CDStreamConfig &config,
                      const Alembic::Abc::ISampleSelector &iss);

#endif  /* __ABC_CUSTOMDATA_H__ */
