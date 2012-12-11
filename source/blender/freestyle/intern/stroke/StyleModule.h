//
//  Filename         : StyleModule.h
//  Author(s)        : Stephane Grabli, Emmanuel Turquin
//  Purpose          : Class representing a style module
//  Date of creation : 01/07/2003
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

#ifndef  STYLE_MODULE_H
# define STYLE_MODULE_H

# include <iostream>
# include <string>
# include "../system/StringUtils.h"
# include "StrokeLayer.h"
# include "../system/Interpreter.h"
# include "Operators.h"
# include "StrokeShader.h"

using namespace std;

class StyleModule
{
public:

  StyleModule(const string& file_name,
	      Interpreter* inter) : _file_name(file_name) {
    _always_refresh = false;
    _causal = false;
    _drawable = true;
    _modified = true;
    _displayed = true;
    _inter = inter;
  }

  virtual ~StyleModule() {}

  StrokeLayer* execute() {
    if (!_inter) {
      cerr << "Error: no interpreter was found to execute the script" << endl;
      return NULL;
    }

	if (!_drawable) {
      cerr << "Error: not drawable" << endl;
      return NULL;
    }

    Operators::reset();

    if( interpret() ) {
      cerr << "Error: interpretation failed" << endl;
      Operators::reset();
      return NULL;
	}
	
    Operators::StrokesContainer* strokes_set = Operators::getStrokesSet();
    if( strokes_set->empty() ) {
   	  cerr << "Error: strokes set empty" << endl;
      Operators::reset();
      return NULL;
	}

    StrokeLayer* sl = new StrokeLayer;
    for (Operators::StrokesContainer::iterator it = strokes_set->begin();
	 it != strokes_set->end();
	 ++it)
      sl->AddStroke(*it);

    Operators::reset();

   return sl;
  }

protected:

  virtual int interpret() {
    return _inter->interpretFile(_file_name);
  }

public:

  // accessors

  const string getFileName() const {
    return _file_name;
  }

  bool getAlwaysRefresh() const {
    return _always_refresh;
  }

  bool getCausal() const {
    return _causal;
  }

  bool getDrawable() const {
    return _drawable;
  }

  bool getModified() const {
    return _modified;
  }

 bool getDisplayed() const {
    return _displayed;
  }

  // modifiers

  void setFileName(const string& file_name) {
    _file_name = file_name;
  }

  void setAlwaysRefresh(bool b = true) {
    _always_refresh = b;
  }

  void setCausal(bool b = true) {
    _causal = b;
  }

  void setDrawable(bool b = true) {
    _drawable = b;
  }

  void setModified(bool b = true) {
    if (_always_refresh)
      return;
    _modified = b;
  }

  void setDisplayed(bool b = true) {
    _displayed = b;
  }

private:

  string	_file_name;
  bool		_always_refresh;
  bool		_causal;
  bool		_drawable;
  bool		_modified;
  bool		_displayed;

protected:

  Interpreter*	_inter;
};

#endif // STYLE_MODULE_H
