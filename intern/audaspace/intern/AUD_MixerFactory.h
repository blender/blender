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

#ifndef AUD_MIXERFACTORY
#define AUD_MIXERFACTORY

#include "AUD_IFactory.h"

/**
 * This factory is a base class for all mixer factories.
 */
class AUD_MixerFactory : public AUD_IFactory
{
protected:
	/**
	 * The target specification for resampling.
	 */
	const AUD_DeviceSpecs m_specs;

	/**
	 * If there is no reader it is created out of this factory.
	 */
	AUD_IFactory* m_factory;

	/**
	 * Returns the reader created out of the factory.
	 * This method can be used for the createReader function of the implementing
	 * classes.
	 * \return The reader to mix.
	 */
	AUD_IReader* getReader() const;

public:
	/**
	 * Creates a new factory.
	 * \param factory The factory to create the readers to mix out of.
	 * \param specs The target specification.
	 */
	AUD_MixerFactory(AUD_IFactory* factory, AUD_DeviceSpecs specs);

	/**
	 * Returns the target specification for resampling.
	 */
	AUD_DeviceSpecs getSpecs() const;

	/**
	 * Returns the saved factory.
	 * \return The factory.
	 */
	AUD_IFactory* getFactory() const;
};

#endif //AUD_MIXERFACTORY
