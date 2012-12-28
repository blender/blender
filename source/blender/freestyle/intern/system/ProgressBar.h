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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_PROGRESS_BAR_H__
#define __FREESTYLE_PROGRESS_BAR_H__

/** \file blender/freestyle/intern/system/ProgressBar.h
 *  \ingroup freestyle
 *  \brief Class to encapsulate a progress bar
 *  \author Stephane Grabli
 *  \date 27/08/2002
 */

#include <string>

using namespace std;

class ProgressBar
{
public:
	inline ProgressBar()
	{
		_numtotalsteps = 0;
		_progress = 0;
	}

	virtual ~ProgressBar() {}

	virtual void reset()
	{
		_numtotalsteps = 0;
		_progress = 0;
	}

	virtual void setTotalSteps(unsigned n)
	{
		_numtotalsteps = n;
	}

	virtual void setProgress(unsigned i)
	{
		_progress = i;
	}

	virtual void setLabelText(const string& s)
	{
		_label = s;
	}

	/*! accessors */
	inline unsigned int getTotalSteps() const
	{
		return _numtotalsteps;
	}

	inline unsigned int getProgress() const
	{
		return _progress;
	}

	inline string getLabelText() const
	{
		return _label;
	}

protected:
	unsigned _numtotalsteps;
	unsigned _progress;
	string _label;
};

#endif // __FREESTYLE_PROGRESS_BAR_H__
