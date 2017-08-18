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

#include "fx/BinauralSound.h"
#include "fx/BinauralReader.h"
#include "Exception.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

BinauralSound::BinauralSound(std::shared_ptr<ISound> sound, std::shared_ptr<HRTF> hrtfs, std::shared_ptr<Source> source, std::shared_ptr<ThreadPool> threadPool) :
	BinauralSound(sound, hrtfs, source, threadPool, std::make_shared<FFTPlan>(0.0))
{
}

BinauralSound::BinauralSound(std::shared_ptr<ISound> sound, std::shared_ptr<HRTF> hrtfs, std::shared_ptr<Source> source, std::shared_ptr<ThreadPool> threadPool, std::shared_ptr<FFTPlan> plan) :
	m_sound(sound), m_hrtfs(hrtfs), m_source(source), m_threadPool(threadPool), m_plan(plan)
{
}

std::shared_ptr<IReader> BinauralSound::createReader()
{
	return std::make_shared<BinauralReader>(m_sound->createReader(), m_hrtfs, m_source, m_threadPool, m_plan);
}

std::shared_ptr<HRTF> BinauralSound::getHRTFs()
{
	return m_hrtfs;
}

void BinauralSound::setHRTFs(std::shared_ptr<HRTF> hrtfs)
{
	m_hrtfs = hrtfs;
}

std::shared_ptr<Source> BinauralSound::getSource()
{
	return m_source;
}

void BinauralSound::setSource(std::shared_ptr<Source> source)
{
	m_source = source;
}

AUD_NAMESPACE_END
