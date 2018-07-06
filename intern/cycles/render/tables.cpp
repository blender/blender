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
 * limitations under the License.
 */

#include "device/device.h"
#include "render/scene.h"
#include "render/tables.h"

#include "util/util_logging.h"

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

void LookupTables::device_update(Device *, DeviceScene *dscene)
{
	if(!need_update)
		return;

	VLOG(1) << "Total " << lookup_tables.size() << " lookup tables.";

	if(lookup_tables.size() > 0)
		dscene->lookup_table.copy_to_device();

	need_update = false;
}

void LookupTables::device_free(Device *, DeviceScene *dscene)
{
	dscene->lookup_table.free();
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
	float *dtable = dscene->lookup_table.data();
	memcpy(dtable + new_table.offset, &data[0], sizeof(float) * data.size());

	return new_table.offset;
}

void LookupTables::remove_table(size_t *offset)
{
	if(*offset == TABLE_OFFSET_INVALID) {
		/* The table isn't even allocated, so just return here. */
		return;
	}

	need_update = true;

	list<Table>::iterator table;

	for(table = lookup_tables.begin(); table != lookup_tables.end(); table++) {
		if(table->offset == *offset) {
			lookup_tables.erase(table);
			*offset = TABLE_OFFSET_INVALID;
			return;
		}
	}

	assert(table != lookup_tables.end());
}

CCL_NAMESPACE_END
