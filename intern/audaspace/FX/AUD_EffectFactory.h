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

/** \file audaspace/FX/AUD_EffectFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_EFFECTFACTORY_H__
#define __AUD_EFFECTFACTORY_H__

#include "AUD_IFactory.h"

/**
 * This factory is a base class for all effect factories that take one other
 * factory as input.
 */
class AUD_EffectFactory : public AUD_IFactory
{
private:
	// hide copy constructor and operator=
	AUD_EffectFactory(const AUD_EffectFactory&);
	AUD_EffectFactory& operator=(const AUD_EffectFactory&);

protected:
	/**
	 * If there is no reader it is created out of this factory.
	 */
	boost::shared_ptr<AUD_IFactory> m_factory;

	/**
	 * Returns the reader created out of the factory.
	 * This method can be used for the createReader function of the implementing
	 * classes.
	 * \return The reader created out of the factory.
	 */
	inline boost::shared_ptr<AUD_IReader> getReader() const
	{
		return m_factory->createReader();
	}

public:
	/**
	 * Creates a new factory.
	 * \param factory The input factory.
	 */
	AUD_EffectFactory(boost::shared_ptr<AUD_IFactory> factory);

	/**
	 * Destroys the factory.
	 */
	virtual ~AUD_EffectFactory();

	/**
	 * Returns the saved factory.
	 * \return The factory or NULL if there has no factory been saved.
	 */
	boost::shared_ptr<AUD_IFactory> getFactory() const;
};

#endif //__AUD_EFFECTFACTORY_H__
