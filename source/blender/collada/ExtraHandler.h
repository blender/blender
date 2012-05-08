/*
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

/** \file blender/collada/ExtraHandler.h
 *  \ingroup collada
 */

#include <string>
#include <map>
#include <vector>
#include <algorithm> // sort()

#include "COLLADASaxFWLIExtraDataCallbackHandler.h"
#include "COLLADASaxFWLFilePartLoader.h"

#include "DocumentImporter.h"
#include "AnimationImporter.h"

/** \brief Handler class for \<extra\> data, through which different
 * profiles can be handled
 */
class ExtraHandler : public COLLADASaxFWL::IExtraDataCallbackHandler
{
public:
	/** Constructor. */
	ExtraHandler(DocumentImporter *dimp, AnimationImporter *aimp);

	/** Destructor. */
	virtual ~ExtraHandler();

	/** Handle the beginning of an element. */
	bool elementBegin( const char* elementName, const char** attributes);
	
	/** Handle the end of an element. */
	bool elementEnd(const char* elementName );
	
	/** Receive the data in text format. */
	bool textData(const char* text, size_t textLength);

	/** Method to ask, if the current callback handler want to read the data of the given extra element. */
	bool parseElement ( 
		const char* profileName, 
		const unsigned long& elementHash, 
		const COLLADAFW::UniqueId& uniqueId );
private:
	/** Disable default copy constructor. */
	ExtraHandler( const ExtraHandler& pre );
	/** Disable default assignment operator. */
	const ExtraHandler& operator= ( const ExtraHandler& pre );
	
	/** Handle to DocumentImporter for interface to extra element data saving. */
	DocumentImporter* dimp;
	AnimationImporter* aimp;
	/** Holds Id of element for which <extra> XML elements are handled. */
	COLLADAFW::UniqueId currentUid;
	ExtraTags* currentExtraTags;
	std::string currentElement;
};

