//
//  Filename         : StrokeIO.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Functions to manage I/O for the stroke
//  Date of creation : 03/02/2004
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

#ifndef  STROKEIO_H
# define STROKEIO_H

# include "Stroke.h"
# include <iostream>
# include "../system/FreestyleConfig.h"

LIB_STROKE_EXPORT
ostream& operator<<(ostream& out, const StrokeAttribute& iStrokeAttribute);

LIB_STROKE_EXPORT
ostream& operator<<(ostream& out, const StrokeVertex& iStrokeVertex);

LIB_STROKE_EXPORT
ostream& operator<<(ostream& out, const Stroke& iStroke);


#endif // STROKEIO_H
