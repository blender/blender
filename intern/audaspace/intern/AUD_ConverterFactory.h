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

#ifndef AUD_CONVERTERFACTORY
#define AUD_CONVERTERFACTORY

#include "AUD_MixerFactory.h"

/**
 * This factory creates a converter reader that is able to convert from one
 * audio format to another.
 */
class AUD_ConverterFactory : public AUD_MixerFactory
{
public:
	AUD_ConverterFactory(AUD_IReader* reader, AUD_Specs specs);
	AUD_ConverterFactory(AUD_IFactory* factory, AUD_Specs specs);
	AUD_ConverterFactory(AUD_Specs specs);

	virtual AUD_IReader* createReader();
};

#endif //AUD_CONVERTERFACTORY
