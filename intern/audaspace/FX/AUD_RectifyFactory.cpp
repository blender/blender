/*
 * $Id: AUD_VolumeFactory.cpp 22328 2009-08-09 23:23:19Z gsrb3d $
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

#include "AUD_RectifyFactory.h"
#include "AUD_RectifyReader.h"

AUD_RectifyFactory::AUD_RectifyFactory(AUD_IFactory* factory) :
		AUD_EffectFactory(factory) {}

AUD_RectifyFactory::AUD_RectifyFactory() :
		AUD_EffectFactory(0) {}

AUD_IReader* AUD_RectifyFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		reader = new AUD_RectifyReader(reader); AUD_NEW("reader")
	}

	return reader;
}
