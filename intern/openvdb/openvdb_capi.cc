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

#include "openvdb_capi.h"
#include "openvdb_dense_convert.h"
#include "openvdb_util.h"

struct OpenVDBFloatGrid { int unused; };
struct OpenVDBIntGrid { int unused; };
struct OpenVDBVectorGrid { int unused; };

int OpenVDB_getVersionHex()
{
	return openvdb::OPENVDB_LIBRARY_VERSION;
}

OpenVDBFloatGrid *OpenVDB_export_grid_fl(
        OpenVDBWriter *writer,
        const char *name, float *data,
		const int res[3], float matrix[4][4], const float clipping,
        OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using openvdb::FloatGrid;

	FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
	FloatGrid *grid = internal::OpenVDB_export_grid<FloatGrid>(
	        writer,
	        name,
	        data,
	        res,
	        matrix,
			clipping,
	        mask_grid);

	return reinterpret_cast<OpenVDBFloatGrid *>(grid);
}

OpenVDBIntGrid *OpenVDB_export_grid_ch(
        OpenVDBWriter *writer,
        const char *name, unsigned char *data,
		const int res[3], float matrix[4][4], const float clipping,
        OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using openvdb::FloatGrid;
	using openvdb::Int32Grid;

	FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
	Int32Grid *grid = internal::OpenVDB_export_grid<Int32Grid>(
	        writer,
	        name,
	        data,
	        res,
	        matrix,
			clipping,
	        mask_grid);

	return reinterpret_cast<OpenVDBIntGrid *>(grid);
}

OpenVDBVectorGrid *OpenVDB_export_grid_vec(struct OpenVDBWriter *writer,
		const char *name,
		const float *data_x, const float *data_y, const float *data_z,
		const int res[3], float matrix[4][4], short vec_type, const float clipping,
		const bool is_color, OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using openvdb::GridBase;
	using openvdb::FloatGrid;
	using openvdb::VecType;

	FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
	GridBase *grid = internal::OpenVDB_export_vector_grid(
	        writer,
	        name,
	        data_x,
	        data_y,
	        data_z,
	        res,
	        matrix,
	        static_cast<VecType>(vec_type),
	        is_color,
			clipping,
	        mask_grid);

	return reinterpret_cast<OpenVDBVectorGrid *>(grid);
}

void OpenVDB_import_grid_fl(
        OpenVDBReader *reader,
        const char *name, float **data,
        const int res[3])
{
	Timer(__func__);

	internal::OpenVDB_import_grid<openvdb::FloatGrid>(reader, name, data, res);
}

void OpenVDB_import_grid_ch(
        OpenVDBReader *reader,
        const char *name, unsigned char **data,
        const int res[3])
{
	internal::OpenVDB_import_grid<openvdb::Int32Grid>(reader, name, data, res);
}

void OpenVDB_import_grid_vec(
        struct OpenVDBReader *reader,
        const char *name,
        float **data_x, float **data_y, float **data_z,
        const int res[3])
{
	Timer(__func__);

	internal::OpenVDB_import_grid_vector(reader, name, data_x, data_y, data_z, res);
}

OpenVDBWriter *OpenVDBWriter_create()
{
	return new OpenVDBWriter();
}

void OpenVDBWriter_free(OpenVDBWriter *writer)
{
	delete writer;
}

void OpenVDBWriter_set_flags(OpenVDBWriter *writer, const int flag, const bool half)
{
	int compression_flags = openvdb::io::COMPRESS_ACTIVE_MASK;

#ifdef WITH_OPENVDB_BLOSC
	if (flag == 0) {
		compression_flags |= openvdb::io::COMPRESS_BLOSC;
	}
	else
#endif
	if (flag == 1) {
		compression_flags |= openvdb::io::COMPRESS_ZIP;
	}
	else {
		compression_flags = openvdb::io::COMPRESS_NONE;
	}

	writer->setFlags(compression_flags, half);
}

void OpenVDBWriter_add_meta_fl(OpenVDBWriter *writer, const char *name, const float value)
{
	writer->insertFloatMeta(name, value);
}

void OpenVDBWriter_add_meta_int(OpenVDBWriter *writer, const char *name, const int value)
{
	writer->insertIntMeta(name, value);
}

void OpenVDBWriter_add_meta_v3(OpenVDBWriter *writer, const char *name, const float value[3])
{
	writer->insertVec3sMeta(name, value);
}

void OpenVDBWriter_add_meta_v3_int(OpenVDBWriter *writer, const char *name, const int value[3])
{
	writer->insertVec3IMeta(name, value);
}

void OpenVDBWriter_add_meta_mat4(OpenVDBWriter *writer, const char *name, float value[4][4])
{
	writer->insertMat4sMeta(name, value);
}

void OpenVDBWriter_write(OpenVDBWriter *writer, const char *filename)
{
	writer->write(filename);
}

OpenVDBReader *OpenVDBReader_create()
{
	return new OpenVDBReader();
}

void OpenVDBReader_free(OpenVDBReader *reader)
{
	delete reader;
}

void OpenVDBReader_open(OpenVDBReader *reader, const char *filename)
{
	reader->open(filename);
}

void OpenVDBReader_get_meta_fl(OpenVDBReader *reader, const char *name, float *value)
{
	reader->floatMeta(name, *value);
}

void OpenVDBReader_get_meta_int(OpenVDBReader *reader, const char *name, int *value)
{
	reader->intMeta(name, *value);
}

void OpenVDBReader_get_meta_v3(OpenVDBReader *reader, const char *name, float value[3])
{
	reader->vec3sMeta(name, value);
}

void OpenVDBReader_get_meta_v3_int(OpenVDBReader *reader, const char *name, int value[3])
{
	reader->vec3IMeta(name, value);
}

void OpenVDBReader_get_meta_mat4(OpenVDBReader *reader, const char *name, float value[4][4])
{
	reader->mat4sMeta(name, value);
}
