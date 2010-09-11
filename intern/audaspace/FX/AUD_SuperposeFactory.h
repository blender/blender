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

#ifndef AUD_SUPERPOSEFACTORY
#define AUD_SUPERPOSEFACTORY

#include "AUD_IFactory.h"

/**
 * This factory plays two other factories behind each other.
 * \note Readers from the underlying factories must have the same sample rate and channel count.
 */
class AUD_SuperposeFactory : public AUD_IFactory
{
private:
	/**
	 * First played factory.
	 */
	AUD_IFactory* m_factory1;

	/**
	 * Second played factory.
	 */
	AUD_IFactory* m_factory2;

	// hide copy constructor and operator=
	AUD_SuperposeFactory(const AUD_SuperposeFactory&);
	AUD_SuperposeFactory& operator=(const AUD_SuperposeFactory&);

public:
	/**
	 * Creates a new superpose factory.
	 * \param factory1 The first input factory.
	 * \param factory2 The second input factory.
	 */
	AUD_SuperposeFactory(AUD_IFactory* factory1, AUD_IFactory* factory2);

	virtual AUD_IReader* createReader() const;
};

#endif //AUD_SUPERPOSEFACTORY
