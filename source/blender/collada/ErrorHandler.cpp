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

/** \file blender/collada/ErrorHandler.cpp
 *  \ingroup collada
 */
#include "ErrorHandler.h"
#include <iostream>

#include "COLLADASaxFWLIError.h"
#include "COLLADASaxFWLSaxParserError.h"
#include "COLLADASaxFWLSaxFWLError.h"

#include "GeneratedSaxParserParserError.h"

#include <string.h>

#include "BLI_utildefines.h"

//--------------------------------------------------------------------
ErrorHandler::ErrorHandler() : mError(false)
{
}

//--------------------------------------------------------------------
ErrorHandler::~ErrorHandler()
{
}

//--------------------------------------------------------------------
bool ErrorHandler::handleError(const COLLADASaxFWL::IError *error)
{
	bool isError = false;
	
	if (error->getErrorClass() == COLLADASaxFWL::IError::ERROR_SAXPARSER) {
		COLLADASaxFWL::SaxParserError *saxParserError = (COLLADASaxFWL::SaxParserError *) error;
		const GeneratedSaxParser::ParserError& parserError = saxParserError->getError();

		// Workaround to avoid wrong error
		if (parserError.getErrorType() == GeneratedSaxParser::ParserError::ERROR_VALIDATION_MIN_OCCURS_UNMATCHED) {
			if (STREQ(parserError.getElement(), "effect")) {
				isError = false;
			}
		}
		if (parserError.getErrorType() == GeneratedSaxParser::ParserError::ERROR_VALIDATION_SEQUENCE_PREVIOUS_SIBLING_NOT_PRESENT) {
			if (!(STREQ(parserError.getElement(), "extra") &&
			      STREQ(parserError.getAdditionalText().c_str(), "sibling: fx_profile_abstract")))
			{
				isError = false;
			}
		}

		if (parserError.getErrorType() == GeneratedSaxParser::ParserError::ERROR_COULD_NOT_OPEN_FILE) {
			std::cout << "Couldn't open file" << std::endl;
		}

		std::cout << "Schema validation error: " << parserError.getErrorMessage() << std::endl;
	}
	else if (error->getErrorClass() == COLLADASaxFWL::IError::ERROR_SAXFWL) {
		COLLADASaxFWL::SaxFWLError *saxFWLError = (COLLADASaxFWL::SaxFWLError *) error;
		/*
		 * Accept non critical errors as warnings (i.e. texture not found)
		 * This makes the importer more graceful, so it now imports what makes sense.
		 */
		isError = (saxFWLError->getSeverity() != COLLADASaxFWL::IError::SEVERITY_ERROR_NONCRITICAL);
		std::cout << "Sax FWL Error: " << saxFWLError->getErrorMessage() << std::endl;
	}
	else {
		std::cout << "opencollada error: " << error->getFullErrorMessage() << std::endl;
	}

	mError |= isError;

	return isError; // let OpenCollada decide when to abort
}
