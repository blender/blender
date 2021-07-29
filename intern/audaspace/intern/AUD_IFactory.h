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

/** \file audaspace/intern/AUD_IFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_IFACTORY_H__
#define __AUD_IFACTORY_H__

#include "AUD_Space.h"
#include "AUD_IReader.h"

#include <boost/shared_ptr.hpp>

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
	virtual ~AUD_IFactory() {}

	/**
	 * Creates a reader for playback of the sound source.
	 * \return A pointer to an AUD_IReader object or NULL if there has been an
	 *         error.
	 * \exception AUD_Exception An exception may be thrown if there has been
	 *            a more unexpected error during reader creation.
	 */
	virtual boost::shared_ptr<AUD_IReader> createReader()=0;
};

#endif //__AUD_IFACTORY_H__
