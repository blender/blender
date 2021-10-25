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

#ifndef __TABLES_H__
#define __TABLES_H__

#include "util/util_list.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

enum { TABLE_CHUNK_SIZE = 256 };
enum { TABLE_OFFSET_INVALID = -1 };

class LookupTables {
public:
	struct Table {
		size_t offset;
		size_t size;
	};

	bool need_update;
	list<Table> lookup_tables;

	LookupTables();
	~LookupTables();

	void device_update(Device *device, DeviceScene *dscene);
	void device_free(Device *device, DeviceScene *dscene);

	size_t add_table(DeviceScene *dscene, vector<float>& data);
	void remove_table(size_t *offset);
};

CCL_NAMESPACE_END

#endif /* __TABLES_H__ */

