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

#ifndef AUD_SILENCEFACTORY
#define AUD_SILENCEFACTORY

#include "AUD_IFactory.h"

/**
 * This factory creates a reader that plays a sine tone.
 */
class AUD_SilenceFactory : public AUD_IFactory
{
private:
	// hide copy constructor and operator=
	AUD_SilenceFactory(const AUD_SilenceFactory&);
	AUD_SilenceFactory& operator=(const AUD_SilenceFactory&);

public:
	/**
	 * Creates a new silence factory.
	 */
	AUD_SilenceFactory();

	virtual AUD_IReader* createReader() const;
};

#endif //AUD_SILENCEFACTORY
