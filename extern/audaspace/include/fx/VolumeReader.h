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
* @file VolumeReader.h
* @ingroup fx
* The VolumeReader class.
*/

#include "IReader.h"
#include "ISound.h"
#include "VolumeStorage.h"

#include <memory>

AUD_NAMESPACE_BEGIN

/**
* This class represents a reader for a sound that has its own shared volume
*/
class AUD_API VolumeReader : public IReader
{
private:
	/**
	* The current reader.
	*/
	std::shared_ptr<IReader> m_reader;

	/**
	* A sound from which to get the reader.
	*/
	std::shared_ptr<VolumeStorage> m_volumeStorage;


	// delete copy constructor and operator=
	VolumeReader(const VolumeReader&) = delete;
	VolumeReader& operator=(const VolumeReader&) = delete;

public:
	/**
	* Creates a new volume reader.
	* \param reader A reader of the sound to be assigned to this reader.
	* \param volumeStorage A shared pointer to a VolumeStorage object.
	*/
	VolumeReader(std::shared_ptr<IReader> reader, std::shared_ptr<VolumeStorage> volumeStorage);

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END