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

/** \file blender/collada/ExtraHandler.cpp
 *  \ingroup collada
 */

#include "BLI_string.h"

#include "ExtraHandler.h"

ExtraHandler::ExtraHandler(){}

ExtraHandler::~ExtraHandler(){}

bool ExtraHandler::elementBegin( const char* elementName, const char** attributes)
{
	printf("begin: %s\n", elementName);
	return true;
}

bool ExtraHandler::elementEnd(const char* elementName )
{
	printf("end: %s\n", elementName);
	return true;
}

bool ExtraHandler::textData(const char* text, size_t textLength)
{
	char buf[1024] = {0};
	_snprintf(buf, textLength, "%s", text);
	printf("data: %s\n", buf);
	return true;
}

bool ExtraHandler::parseElement ( 
	const char* profileName, 
	const unsigned long& elementHash, 
	const COLLADAFW::UniqueId& uniqueId ) {
		if(BLI_strcaseeq(profileName, "blender")) {
			printf("In parseElement for supported profile %s for id %s\n", profileName, uniqueId.toAscii().c_str());
			return true;
		}
		printf("In parseElement for unsupported profile %s for id %s\n", profileName, uniqueId.toAscii().c_str());
		return false;
}
