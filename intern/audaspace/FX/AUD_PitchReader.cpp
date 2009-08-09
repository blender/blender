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

#include "AUD_PitchReader.h"

AUD_PitchReader::AUD_PitchReader(AUD_IReader* reader, float pitch) :
		AUD_EffectReader(reader)
{
	m_pitch = pitch;
}

AUD_Specs AUD_PitchReader::getSpecs()
{
	AUD_Specs specs = m_reader->getSpecs();
	specs.rate = (AUD_SampleRate)((int)(specs.rate * m_pitch));
	return specs;
}
