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

#ifndef AUD_SDLMIXERREADER
#define AUD_SDLMIXERREADER

#include "AUD_IReader.h"
class AUD_Buffer;

#include <SDL.h>

/**
 * This class mixes a sound source with help of the SDL library.
 * Unfortunately SDL is only capable of 8 and 16 bit audio, mono and stereo, as
 * well as resampling only 2^n sample rate relationships where n is a natural
 * number.
 * \warning Although SDL can only resample 2^n sample rate relationships, this
 *          class doesn't check for compliance, so in case of other factors,
 *          the behaviour is undefined.
 */
class AUD_SDLMixerReader : public AUD_IReader
{
private:
	/**
	 * The reader that is being mixed.
	 */
	AUD_IReader* m_reader;

	/**
	 * The current reading position in the resampling buffer.
	 */
	int m_rsposition;

	/**
	 * The count of mixed samples in the resampling buffer.
	 */
	int m_rssize;

	/**
	 * The smallest count of source samples to get a fractionless resampling
	 * factor.
	 */
	int m_ssize;

	/**
	 * The smallest count of target samples to get a fractionless resampling
	 * factor.
	 */
	int m_tsize;

	/**
	 * The sound output buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The resampling buffer.
	 */
	AUD_Buffer *m_rsbuffer;

	/**
	 * The target specification.
	 */
	AUD_Specs m_tspecs;

	/**
	 * The sample specification of the source.
	 */
	AUD_Specs m_sspecs;

	/**
	 * Saves whether the end of the source has been reached.
	 */
	bool m_eor;

	/**
	 * The SDL_AudioCVT structure used for resampling.
	 */
	SDL_AudioCVT m_cvt;

public:
	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param specs The target specification.
	 * \exception AUD_Exception Thrown if the source specification cannot be
	 *            mixed to the target specification or if the reader is
	 *            NULL.
	 */
	AUD_SDLMixerReader(AUD_IReader* reader, AUD_Specs specs);
	/**
	 * Destroys the reader.
	 */
	~AUD_SDLMixerReader();

	virtual bool isSeekable();
	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual AUD_Specs getSpecs();
	virtual AUD_ReaderType getType();
	virtual bool notify(AUD_Message &message);
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_SDLMIXERREADER
