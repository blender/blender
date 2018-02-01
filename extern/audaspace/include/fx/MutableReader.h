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

#pragma once

/**
* @file MutableReader.h
* @ingroup fx
* The MutableReader class.
*/

#include "IReader.h"
#include "ISound.h"

#include <memory>

AUD_NAMESPACE_BEGIN

/**
* This class represents a reader for a sound that can change with each playback. The change will occur when trying to seek backwards
* If the sound doesn't support that, it will be restarted.
* \warning Notice that if a SoundList object is assigned to several MutableReaders, sequential playback won't work correctly.
*          To prevent this the SoundList must be copied.
*/
class AUD_API MutableReader : public IReader
{
private:
	/**
	* The current reader.
	*/
	std::shared_ptr<IReader> m_reader;

	/**
	* A sound from which to get the reader.
	*/
	std::shared_ptr<ISound> m_sound;


	// delete copy constructor and operator=
	MutableReader(const MutableReader&) = delete;
	MutableReader& operator=(const MutableReader&) = delete;

public:
	/**
	* Creates a new mutable reader.
	* \param sound A of sound you want to assign to this reader.
	*/
	MutableReader(std::shared_ptr<ISound> sound);

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
