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

/** \file audaspace/FX/AUD_LimiterFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_LIMITERFACTORY_H__
#define __AUD_LIMITERFACTORY_H__

#include "AUD_EffectFactory.h"

/**
 * This factory limits another factory in start and end time.
 */
class AUD_LimiterFactory : public AUD_EffectFactory
{
private:
	/**
	 * The start time.
	 */
	const float m_start;

	/**
	 * The end time.
	 */
	const float m_end;

	// hide copy constructor and operator=
	AUD_LimiterFactory(const AUD_LimiterFactory&);
	AUD_LimiterFactory& operator=(const AUD_LimiterFactory&);

public:
	/**
	 * Creates a new limiter factory.
	 * \param factory The input factory.
	 * \param start The desired start time.
	 * \param end The desired end time, a negative value signals that it should
	 *            play to the end.
	 */
	AUD_LimiterFactory(boost::shared_ptr<AUD_IFactory> factory,
					   float start = 0, float end = -1);

	/**
	 * Returns the start time.
	 */
	float getStart() const;

	/**
	 * Returns the end time.
	 */
	float getEnd() const;

	virtual boost::shared_ptr<AUD_IReader> createReader();
};

#endif //__AUD_LIMITERFACTORY_H__
