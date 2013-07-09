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

/** \file audaspace/intern/AUD_SilenceFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SILENCEFACTORY_H__
#define __AUD_SILENCEFACTORY_H__

#include "AUD_IFactory.h"

/**
 * This factory creates a reader that plays silence.
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

	virtual boost::shared_ptr<AUD_IReader> createReader();
};

#endif //__AUD_SILENCEFACTORY_H__
