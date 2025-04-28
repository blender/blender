/*******************************************************************************
 * Copyright 2009-2024 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#define PIPEWIRE_LIBRARY_IMPLEMENTATION

#include <string>
#include <array>

#include "PipeWireLibrary.h"

#ifdef DYNLOAD_PIPEWIRE
#include "plugin/PluginManager.h"
#endif

AUD_NAMESPACE_BEGIN

bool loadPipeWire()
{
#ifdef DYNLOAD_PIPEWIRE
	std::array<const std::string, 2> names = {"libpipewire-0.3.so", "libpipewire-0.3.so.0"};

	void* handle = nullptr;

	for(auto& name : names)
	{
		handle = PluginManager::openLibrary(name);
		if(handle)
			break;
	}

	if (!handle)
		return false;

#define PIPEWIRE_SYMBOL(sym) AUD_##sym = reinterpret_cast<decltype(&sym)>(PluginManager::lookupLibrary(handle, #sym))
#else
#define PIPEWIRE_SYMBOL(sym) AUD_##sym = &sym
#endif

#include "PipeWireSymbols.h"

#undef PIPEWIRE_SYMBOL

	return AUD_pw_check_library_version != nullptr && AUD_pw_check_library_version(1, 1, 0);
}

AUD_NAMESPACE_END
