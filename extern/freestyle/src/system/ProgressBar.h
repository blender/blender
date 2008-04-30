//
//  Filename         : ProgressBar.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to encapsulate a progress bar
//  Date of creation : 27/08/2002
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  PROGRESSBAR_H
# define PROGRESSBAR_H

# include <string>

using namespace std;

class ProgressBar
{
public:

  inline ProgressBar() {
    _numtotalsteps = 0;
    _progress = 0;
  }

  virtual ~ProgressBar() {}

  virtual void reset() {
    _numtotalsteps = 0;
    _progress = 0;
  }

  virtual void setTotalSteps(unsigned n) {
    _numtotalsteps = n;
  }

  virtual void setProgress(unsigned i) {
    _progress = i;
  }

  virtual void setLabelText(const string& s) {
    _label = s;
  }

  /*! accessors */
  inline unsigned int getTotalSteps() const {
    return _numtotalsteps;
  }

  inline unsigned int getProgress() const {
    return _progress;
  }

  inline string getLabelText() const {
    return _label;
  }
 
protected:

  unsigned _numtotalsteps;
  unsigned _progress;
  string _label;
};

#endif // PROGRESSBAR_H
