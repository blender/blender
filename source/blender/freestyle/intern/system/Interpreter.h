//
//  Filename         : Interpreter.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : Base Class of all script interpreters
//  Date of creation : 17/04/2003
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

#ifndef  INTERPRETER_H
# define INTERPRETER_H

# include <string>

using namespace std;

class LIB_SYSTEM_EXPORT Interpreter
{
 public:

  Interpreter() { _language = "Unknown"; }

  virtual ~Interpreter() {}; //soc

  virtual int interpretFile(const string& filename) = 0;

  virtual string getLanguage() const { return _language; }

  virtual void reset() = 0;

 protected:

  string _language;
};

#endif // INTERPRETER_H
