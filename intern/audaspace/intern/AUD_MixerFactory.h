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

/** \file audaspace/intern/AUD_MixerFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_MIXERFACTORY_H__
#define __AUD_MIXERFACTORY_H__

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
	AUD_Reference<AUD_IFactory> m_factory;

	/**
	 * Returns the reader created out of the factory.
	 * This method can be used for the createReader function of the implementing
	 * classes.
	 * \return The reader to mix.
	 */
	AUD_Reference<AUD_IReader> getReader() const;

public:
	/**
	 * Creates a new factory.
	 * \param factory The factory to create the readers to mix out of.
	 * \param specs The target specification.
	 */
	AUD_MixerFactory(AUD_Reference<AUD_IFactory> factory, AUD_DeviceSpecs specs);

	/**
	 * Returns the target specification for resampling.
	 */
	AUD_DeviceSpecs getSpecs() const;

	/**
	 * Returns the saved factory.
	 * \return The factory.
	 */
	AUD_Reference<AUD_IFactory> getFactory() const;
};

#endif //__AUD_MIXERFACTORY_H__
