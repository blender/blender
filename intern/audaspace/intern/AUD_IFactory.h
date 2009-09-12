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

#ifndef AUD_IFACTORY
#define AUD_IFACTORY

#include "AUD_Space.h"
class AUD_IReader;

/**
 * This class represents a type of sound source and saves the necessary values
 * for it. It is able to create a reader that is actually usable for playback
 * of the respective sound source through the factory method createReader.
 */
class AUD_IFactory
{
public:
	/**
	 * Destroys the factory.
	 */
	virtual ~AUD_IFactory(){}

	/**
	 * Creates a reader for playback of the sound source.
	 * \return A pointer to an AUD_IReader object or NULL if there has been an
	 *         error.
	 * \exception AUD_Exception An exception may be thrown if there has been
	 *            a more unexpected error during reader creation.
	 */
	virtual AUD_IReader* createReader()=0;
};

#endif //AUD_IFACTORY
