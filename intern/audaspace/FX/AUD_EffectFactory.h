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

#ifndef AUD_EFFECTFACTORY
#define AUD_EFFECTFACTORY

#include "AUD_IFactory.h"

/**
 * This factory is a base class for all effect factories that take one other
 * factory as input.
 */
class AUD_EffectFactory : public AUD_IFactory
{
protected:
	/**
	 * If there is no reader it is created out of this factory.
	 */
	AUD_IFactory* m_factory;

	/**
	 * Returns the reader created out of the factory.
	 * This method can be used for the createReader function of the implementing
	 * classes.
	 * \return The reader created out of the factory or NULL if there is none.
	 */
	AUD_IReader* getReader();

public:
	/**
	 * Creates a new factory.
	 * \param factory The input factory.
	 */
	AUD_EffectFactory(AUD_IFactory* factory);

	/**
	 * Destroys the factory.
	 */
	virtual ~AUD_EffectFactory() {}

	/**
	 * Sets the input factory.
	 * \param factory The input factory.
	 */
	void setFactory(AUD_IFactory* factory);

	/**
	 * Returns the saved factory.
	 * \return The factory or NULL if there has no factory been saved.
	 */
	AUD_IFactory* getFactory();
};

#endif //AUD_EFFECTFACTORY
