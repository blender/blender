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

/** \file blender/collada/ErrorHandler.h
 *  \ingroup collada
 */

#include <string>
#include <map>
#include <vector>
#include <algorithm> // sort()

#include "COLLADASaxFWLIErrorHandler.h"

/** \brief Handler class for parser errors
 */
class ErrorHandler : public COLLADASaxFWL::IErrorHandler
{
public:
	/** Constructor. */
	ErrorHandler();

	/** Destructor. */
	virtual ~ErrorHandler();
	/** handle any error thrown by the parser. */
	bool virtual handleError(const COLLADASaxFWL::IError* error);
	/** True if there was an error during parsing. */
	bool hasError() { return mError; }
private:
	/** Disable default copy ctor. */
	ErrorHandler(const ErrorHandler& pre);
	/** Disable default assignment operator. */
	const ErrorHandler& operator= ( const ErrorHandler& pre );
	/** Hold error status. */
	bool mError;
};

