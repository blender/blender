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

#include <stdlib.h>

#include "util_dynlib.h"

#ifdef _WIN32

#include <windows.h>

CCL_NAMESPACE_BEGIN

struct DynamicLibrary {
	HMODULE module;
};

DynamicLibrary *dynamic_library_open(const char *name)
{
	HMODULE module = LoadLibrary(name);

	if(!module)
		return NULL;

	DynamicLibrary *lib = new DynamicLibrary();
	lib->module = module;

	return lib;
}

void *dynamic_library_find(DynamicLibrary *lib, const char *name)
{
	return (void*)GetProcAddress(lib->module, name);
}

void dynamic_library_close(DynamicLibrary *lib)
{
	FreeLibrary(lib->module);
	delete lib;
}

CCL_NAMESPACE_END

#else

#include <dlfcn.h>

CCL_NAMESPACE_BEGIN

struct DynamicLibrary {
	void *module;
};

DynamicLibrary *dynamic_library_open(const char *name)
{
	void *module = dlopen(name, RTLD_NOW);

	if(!module)
		return NULL;

	DynamicLibrary *lib = new DynamicLibrary();
	lib->module = module;

	return lib;
}

void *dynamic_library_find(DynamicLibrary *lib, const char *name)
{
	return dlsym(lib->module, name);
}

void dynamic_library_close(DynamicLibrary *lib)
{
	dlclose(lib->module);
	delete lib;
}

CCL_NAMESPACE_END

#endif

