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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2006 The Zdeno Ash Miklas
 *
 * This source file is part of VideoTexture library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/VideoTexture/Exception.cpp
 *  \ingroup bgevideotex
 */

#include <sstream>
#include <fstream>

#include "PyObjectPlus.h"

#include "Exception.h"


// exception identificators
ExceptionID ErrGeneral, ErrNotFound;

// exception descriptions
ExpDesc errGenerDesc(ErrGeneral, "General Error");
ExpDesc errNFoundDesc(ErrNotFound, "Error description not found");



// implementation of ExpDesc

// constructor
ExpDesc::ExpDesc (ExceptionID & exp, const char *desc, RESULT hres)
: m_expID(exp), m_hRslt(hres), m_description(desc)
{
}

// destructor
ExpDesc::~ExpDesc (void) {}

// list of descriptions
std::vector<ExpDesc*> ExpDesc::m_expDescs;


// class Exception


// last exception description
std::string Exception::m_lastError;

// log file name
const char * Exception::m_logFile = NULL;


// basic constructor
Exception::Exception ()
{
	// default values
	m_expID = &ErrNotFound;
	m_hRslt = S_OK;
	m_line = 0;
}


// destructor
Exception::~Exception () throw() { }


// copy constructor
Exception::Exception (const Exception & xpt)
{ copy (xpt); }


// assignment operator
Exception & Exception::operator= (const Exception & xpt)
{ copy (xpt); return *this; }


// get exception description
const char * Exception::what()
{
	// set exception description
	setXptDesc();
	// return c string
	return m_desc.c_str();
}


// debug version - with file and line of exception
Exception::Exception (ExceptionID & expID, RESULT rslt, const char *fil, int lin)
: m_expID (&expID), m_hRslt (rslt)
{
	// set file and line
	if (fil[0] != '\0' || lin > 0)
		setFileLine (fil, lin);
	else
		m_line = -1;
}


// set file and line
void Exception::setFileLine (const char *fil, int lin)
{
	if (fil != NULL) m_fileName = fil;
	m_line = lin;
}


// report exception
void Exception::report(void)
{
	// set exception description
	setXptDesc();
	// set python error
	PyErr_SetString(PyExc_RuntimeError, what());
	// if log file is set
	if (m_logFile != NULL)
	{
		// write description to log
		std::ofstream logf (m_logFile, std::ios_base::app);
		logf << m_fileName << ':' << m_line << ':' << m_desc << std::endl;
		logf.flush();
		logf.close();
	}
}


// set exception description
void Exception::setXptDesc (void)
{
	// if description is not set
	if (m_desc.size() == 0)
	{
		// start of search                           -1
		// found description "NotFound"               0
		// found description without matching result  1
		// found description with matching result     2
		int best = -1;
		// find exception description
		for (std::vector<ExpDesc*>::iterator it = ExpDesc::m_expDescs.begin(); it != ExpDesc::m_expDescs.end(); ++it)
		{
			// use "NotFound", if there is not better
			if (best < 0 && (*it)->isExp(&ErrNotFound) > 0)
			{
				(*it)->loadDesc(m_desc);
				best = 0;
			}
			// match exception
			int nBest = (*it)->isExp(m_expID, m_hRslt);
			// if exception is matching better
			if (nBest > 0 && best < nBest)
			{
				// set description
				(*it)->loadDesc(m_desc);
				best = nBest;
				// if matching exactly, finish search
				if (best == 2) break;
			}
		}
		// add result code
		// length of result code
		const size_t rsltSize = 11;
		// delimit description
		//const char delimRslt[] = ": ";
		// set text of description
		char rsltTxt[rsltSize];
		std::ostringstream os;
		os << std::hex << m_hRslt << ": " << '\0';
		// copy result to description
		m_desc.insert(0, rsltTxt);
		// copy exception description to last exception string
		m_lastError = m_desc;
	}
}


// copy exception data
void Exception::copy (const Exception & xpt)
{
	// standard data
	m_expID = xpt.m_expID;
	m_hRslt = xpt.m_hRslt;
	m_desc = xpt.m_desc;

	// debug data
	m_fileName = xpt.m_fileName;
	m_line = xpt.m_line;
}

void registerAllExceptions(void)
{
	errGenerDesc.registerDesc();
	errNFoundDesc.registerDesc();
	MaterialNotAvailDesc.registerDesc();
	ImageSizesNotMatchDesc.registerDesc();
	ImageHasExportsDesc.registerDesc();
	InvalidColorChannelDesc.registerDesc();
	SceneInvalidDesc.registerDesc();
	CameraInvalidDesc.registerDesc();
	ObserverInvalidDesc.registerDesc();
	MirrorInvalidDesc.registerDesc();
	MirrorSizeInvalidDesc.registerDesc();
	MirrorNormalInvalidDesc.registerDesc();
	MirrorHorizontalDesc.registerDesc();
	MirrorTooSmallDesc.registerDesc();
	SourceVideoEmptyDesc.registerDesc();
	SourceVideoCreationDesc.registerDesc();
}
