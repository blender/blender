/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/ExtraTags.cpp
 *  \ingroup collada
 */

#include <stddef.h>
#include "BLI_string.h"

#include <iostream>

#include "ExtraTags.h"

ExtraTags::ExtraTags(const std::string profile)
{
	this->profile = profile;
}

ExtraTags::~ExtraTags()
{
}

bool ExtraTags::addTag(const std::string tag, const std::string data)
{
	//std::cout << "ready to add " << tag << ": " << data << "." << std::endl;
	
	return true;
}
