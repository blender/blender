/*
 * Copyright 2011, Blender Foundation.
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
 */

#include "device.h"
#include "scene.h"
#include "tables.h"

#include "util_debug.h"

CCL_NAMESPACE_BEGIN

/* Lookup Tables */

LookupTables::LookupTables()
{
	need_update = true;
}

LookupTables::~LookupTables()
{
	assert(lookup_tables.size() == 0);
}

void LookupTables::device_update(Device *device, DeviceScene *dscene)
{
	if(!need_update)
		return;

	device->tex_alloc("__lookup_table", dscene->lookup_table);

	need_update = false;
}

void LookupTables::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->lookup_table);
	dscene->lookup_table.clear();
}

static size_t round_up_to_multiple(size_t size, size_t chunk)
{
	return ((size + chunk - 1)/chunk) * chunk;
}

size_t LookupTables::add_table(DeviceScene *dscene, vector<float>& data)
{
	assert(data.size() > 0);

	need_update = true;

	Table new_table;
	new_table.offset = 0;
	new_table.size = round_up_to_multiple(data.size(), TABLE_CHUNK_SIZE);

	/* find space to put lookup table */
	list<Table>::iterator table;

	for(table = lookup_tables.begin(); table != lookup_tables.end(); table++) {
		if(new_table.offset + new_table.size <= table->offset) {
			lookup_tables.insert(table, new_table);
			break;
		}
		else
			new_table.offset = table->offset + table->size;
	}

	if(table == lookup_tables.end()) {
		/* add at the end */
		lookup_tables.push_back(new_table);
		dscene->lookup_table.resize(new_table.offset + new_table.size);
	}

	/* copy table data and return offset */
	dscene->lookup_table.copy_at(&data[0], new_table.offset, data.size());
	return new_table.offset;
}

void LookupTables::remove_table(size_t offset)
{
	need_update = true;

	list<Table>::iterator table;

	for(table = lookup_tables.begin(); table != lookup_tables.end(); table++) {
		if(table->offset == offset) {
			lookup_tables.erase(table);
			break;
		}
	}

	assert(table != lookup_tables.end());
}

CCL_NAMESPACE_END

