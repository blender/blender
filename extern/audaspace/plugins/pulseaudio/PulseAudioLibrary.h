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

#pragma once

#ifdef PULSEAUDIO_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file PulseAudioLibrary.h
 * @ingroup plugin
 */

#include "Audaspace.h"

#include <pulse/pulseaudio.h>

AUD_NAMESPACE_BEGIN

#ifdef PULSEAUDIO_LIBRARY_IMPLEMENTATION
#define PULSEAUDIO_SYMBOL(sym) decltype(&sym) AUD_##sym
#else
#define PULSEAUDIO_SYMBOL(sym) extern decltype(&sym) AUD_##sym
#endif

#include "PulseAudioSymbols.h"

#undef PULSEAUDIO_SYMBOL

bool loadPulseAudio();

AUD_NAMESPACE_END
