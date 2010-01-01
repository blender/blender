/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_SRCRESAMPLEREADER
#define AUD_SRCRESAMPLEREADER

#include "AUD_EffectReader.h"
class AUD_Buffer;

#include <samplerate.h>

/**
 * This resampling reader uses libsamplerate for resampling.
 */
class AUD_SRCResampleReader : public AUD_EffectReader
{
private:
	/**
	 * The resampling factor.
	 */
	double m_factor;

	/**
	 * The sound output buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The target specification.
	 */
	AUD_Specs m_tspecs;

	/**
	 * The sample specification of the source.
	 */
	AUD_Specs m_sspecs;

	/**
	 * The src state structure.
	 */
	SRC_STATE* m_src;

public:
	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param specs The target specification.
	 * \exception AUD_Exception Thrown if the source specification cannot be
	 *            mixed to the target specification or if the reader is
	 *            NULL.
	 */
	AUD_SRCResampleReader(AUD_IReader* reader, AUD_Specs specs);

	/**
	 * Destroys the reader.
	 */
	~AUD_SRCResampleReader();

	/**
	 * The callback function for SRC.
	 * \warning Do not call!
	 * \param data The pointer to the float data.
	 * \return The count of samples in the float data.
	 */
	long doCallback(float** data);

	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual AUD_Specs getSpecs();
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_SRCRESAMPLEREADER
