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

#include "AUD_LoopFactory.h"
#include "AUD_LoopReader.h"

AUD_LoopFactory::AUD_LoopFactory(AUD_IFactory* factory, int loop) :
		AUD_EffectFactory(factory),
		m_loop(loop)
{
}

int AUD_LoopFactory::getLoop() const
{
	return m_loop;
}

AUD_IReader* AUD_LoopFactory::createReader() const
{
	return new AUD_LoopReader(getReader(), m_loop);
}
