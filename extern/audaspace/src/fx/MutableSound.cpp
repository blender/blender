/*******************************************************************************
* Copyright 2015-2016 Juan Francisco Crespo Galán
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

#include "fx/MutableSound.h"
#include "fx/MutableReader.h"
#include "Exception.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

MutableSound::MutableSound(std::shared_ptr<ISound> sound) : 
m_sound(sound)
{
}

std::shared_ptr<IReader> MutableSound::createReader()
{
	return std::make_shared<MutableReader>(m_sound);
}

AUD_NAMESPACE_END