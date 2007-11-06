/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef DT_ALGOTABLE_H
#define DT_ALGOTABLE_H

#include "DT_Shape.h"

template <typename Function, int NUM_TYPES = 8>
class AlgoTable {
public:
  void addEntry(DT_ShapeType type1, DT_ShapeType type2, Function function) 
  { 
    table[type2][type1] = function;
    table[type1][type2] = function;
  }

  Function lookup(DT_ShapeType type1, DT_ShapeType type2) const 
  {
    return table[type1][type2];
  }

private:
  Function table[NUM_TYPES][NUM_TYPES];
};

#endif
