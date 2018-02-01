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

/**
 * @file IReader.h
 * @ingroup general
 * The IReader interface.
 */

#include "respec/Specification.h"

AUD_NAMESPACE_BEGIN

/**
 * @interface IReader
 * This class represents a sound source as stream or as buffer which can be read
 * for example by another reader, a device or whatever.
 */
class AUD_API IReader
{
public:
	/**
	 * Destroys the reader.
	 */
	virtual ~IReader() {}

	/**
	 * Tells whether the source provides seeking functionality or not.
	 * \warning This doesn't mean that the seeking always has to succeed.
	 * \return Always returns true for readers of buffering types.
	 */
	virtual bool isSeekable() const=0;

	/**
	 * Seeks to a specific position in the source.
	 * \param position The position to seek for measured in samples. To get
	 *        from a given time to the samples you simply have to multiply the
	 *        time value in seconds with the sample rate of the reader.
	 * \warning This may work or not, depending on the actual reader.
	 */
	virtual void seek(int position)=0;

	/**
	 * Returns an approximated length of the source in samples.
	 * \return The length as sample count. May be negative if unknown.
	 */
	virtual int getLength() const=0;

	/**
	 * Returns the position of the source as a sample count value.
	 * \return The current position in the source. A negative value indicates
	 *         that the position is unknown.
	 * \warning The value returned doesn't always have to be correct for readers,
	 *          especially after seeking.
	 */
	virtual int getPosition() const=0;

	/**
	 * Returns the specification of the reader.
	 * \return The Specs structure.
	 */
	virtual Specs getSpecs() const=0;

	/**
	 * Request to read the next length samples out of the source.
	 * The buffer supplied has the needed size.
	 * \param[in,out] length The count of samples that should be read. Shall
	 *                contain the real count of samples after reading, in case
	 *                there were only fewer samples available.
	 *                A smaller value also indicates the end of the reader.
	 * \param[out] eos End of stream, whether the end is reached or not.
	 * \param[in] buffer The pointer to the buffer to read into.
	 */
	virtual void read(int& length, bool& eos, sample_t* buffer)=0;
};

AUD_NAMESPACE_END
