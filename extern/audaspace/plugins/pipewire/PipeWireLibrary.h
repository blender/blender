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

#pragma once

#ifdef PIPEWIRE_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file PipeWireLibrary.h
 * @ingroup plugin
 */

#include "Audaspace.h"

#include <pipewire/pipewire.h>
#include <pipewire/stream.h>

AUD_NAMESPACE_BEGIN

#ifdef PIPEWIRE_LIBRARY_IMPLEMENTATION
#define PIPEWIRE_SYMBOL(sym) decltype(&sym) AUD_##sym
#else
#define PIPEWIRE_SYMBOL(sym) extern decltype(&sym) AUD_##sym
#endif

#include "PipeWireSymbols.h"

#undef PIPEWIRE_SYMBOL

bool loadPipeWire();

AUD_NAMESPACE_END
