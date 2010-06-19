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

#ifndef AUD_SRCRESAMPLEFACTORY
#define AUD_SRCRESAMPLEFACTORY

#include "AUD_ResampleFactory.h"

/**
 * This factory creates a resampling reader that uses libsamplerate for
 * resampling.
 */
class AUD_SRCResampleFactory : public AUD_ResampleFactory
{
public:
	AUD_SRCResampleFactory(AUD_IReader* reader, AUD_DeviceSpecs specs);
	AUD_SRCResampleFactory(AUD_IFactory* factory, AUD_DeviceSpecs specs);
	AUD_SRCResampleFactory(AUD_DeviceSpecs specs);

	virtual AUD_IReader* createReader();
};

#endif //AUD_SRCRESAMPLEFACTORY
