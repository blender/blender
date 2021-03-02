/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
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

#define PULSEAUDIO_LIBRARY_IMPLEMENTATION

#include <string>
#include <array>

#include "PulseAudioLibrary.h"

#ifdef DYNLOAD_PULSEAUDIO
#include "plugin/PluginManager.h"
#endif

AUD_NAMESPACE_BEGIN

bool loadPulseAudio()
{
#ifdef DYNLOAD_PULSEAUDIO
	std::array<const std::string, 2> names = {"libpulse.so", "libpulse.so.0"};

	void* handle = nullptr;

	for(auto& name : names)
	{
		handle = PluginManager::openLibrary(name);
		if(handle)
			break;
	}

	if (!handle)
		return false;

#define PULSEAUDIO_SYMBOL(sym) AUD_##sym = reinterpret_cast<decltype(&sym)>(PluginManager::lookupLibrary(handle, #sym))
#else
#define PULSEAUDIO_SYMBOL(sym) AUD_##sym = &sym
#endif

#include "PulseAudioSymbols.h"

#undef PULSEAUDIO_SYMBOL

	return AUD_pa_context_new != nullptr;
}

AUD_NAMESPACE_END
