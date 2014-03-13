/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
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

	device->tex_free(dscene->lookup_table);

	if(lookup_tables.size() > 0)
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
			return;
		}
	}

	assert(table != lookup_tables.end());
}

CCL_NAMESPACE_END

