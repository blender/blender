/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_ResampleReader.h
 *  \ingroup audaspaceintern
 */

#ifndef __AUD_RESAMPLEREADER_H__
#define __AUD_RESAMPLEREADER_H__

#include "AUD_EffectReader.h"

/**
 * This is the base class for all resampling readers.
 */
class AUD_ResampleReader : public AUD_EffectReader
{
protected:
	/**
	 * The target sampling rate.
	 */
	AUD_SampleRate m_rate;

	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param rate The target sampling rate.
	 */
	AUD_ResampleReader(boost::shared_ptr<AUD_IReader> reader, AUD_SampleRate rate);

public:
	/**
	 * Sets the sample rate.
	 * \param rate The target sampling rate.
	 */
	virtual void setRate(AUD_SampleRate rate);

	/**
	 * Retrieves the sample rate.
	 * \return The target sampling rate.
	 */
	virtual AUD_SampleRate getRate();
};

#endif // __AUD_RESAMPLEREADER_H__
