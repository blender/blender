/**
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include "BOP_Tag.h"

/**
 * Gets the tag description.
 * @param t tag
 * @param dest tag description
 */
void BOP_stringTAG(BOP_TAG t, char *dest) {
  
  switch(t){	
  case IN_IN_IN:
    sprintf(dest, "IN_IN_IN");
    break;
  case IN_IN_ON:
    sprintf(dest, "IN_IN_ON");
    break;
  case IN_ON_IN:
    sprintf(dest, "IN_ON_IN");
    break;
  case IN_ON_ON:
    sprintf(dest, "IN_ON_ON");
    break;
  case ON_IN_IN:
    sprintf(dest, "ON_IN_IN");
    break;
  case ON_IN_ON:
    sprintf(dest, "ON_IN_ON");
    break;
  case ON_ON_IN:
    sprintf(dest, "ON_ON_IN");
    break;
  case ON_ON_ON:
    sprintf(dest, "ON_ON_ON");
    break;    
  case OUT_OUT_OUT:
    sprintf(dest, "OUT_OUT_OUT");
    break;
  case OUT_OUT_ON:
    sprintf(dest, "OUT_OUT_ON");
    break;
  case OUT_ON_OUT:
    sprintf(dest, "OUT_ON_OUT");
    break;
  case OUT_ON_ON:
    sprintf(dest, "OUT_ON_ON");
    break;
  case ON_OUT_OUT:
    sprintf(dest, "ON_OUT_OUT");
    break;
  case ON_OUT_ON:
    sprintf(dest, "ON_OUT_ON");
    break;
  case ON_ON_OUT:
    sprintf(dest, "ON_ON_OUT");
    break;    
  case OUT_OUT_IN:
    sprintf(dest, "OUT_OUT_IN");
    break;
  case OUT_IN_OUT:
    sprintf(dest, "OUT_IN_OUT");
    break;
  case OUT_IN_IN:
    sprintf(dest, "OUT_IN_IN");
    break;
  case IN_OUT_OUT:
    sprintf(dest, "IN_OUT_OUT");
    break;
  case IN_OUT_IN:
    sprintf(dest, "IN_OUT_IN");
    break;
  case IN_IN_OUT:
    sprintf(dest, "IN_IN_OUT");
    break;    
  case OUT_ON_IN:
    sprintf(dest, "OUT_ON_IN");
    break;
  case OUT_IN_ON:
    sprintf(dest, "OUT_IN_ON");
    break;
  case IN_ON_OUT:
    sprintf(dest, "IN_ON_OUT");
    break;
  case IN_OUT_ON:
    sprintf(dest, "IN_OUT_ON");
    break;
  case ON_IN_OUT:
    sprintf(dest, "ON_IN_OUT");
    break;
  case ON_OUT_IN:
    sprintf(dest, "ON_OUT_IN");
    break;
  case UNCLASSIFIED:
    sprintf(dest, "UNCLASSIFIED");	
    break;
  case BROKEN:
    sprintf(dest, "BROKEN");
    break;
  case PHANTOM:
    sprintf(dest, "PHANTOM");
    break;
  case OVERLAPPED:
    sprintf(dest, "OVERLAPPED");
    break;
  case INOUT:
    sprintf(dest, "INOUT");
    break;    
  default:
    sprintf(dest, "DESCONEGUT %d",t);
    break;
  }
  
}
