/**
 * $Id$
 *
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
#include "EXP_C-Api.h"
#include "IntValue.h"
#include "BoolValue.h"
#include "StringValue.h"
#include "ErrorValue.h"
#include "InputParser.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

EXP_ValueHandle EXP_CreateInt(int innie)
{
	return (EXP_ValueHandle) new CIntValue(innie);
}



EXP_ValueHandle EXP_CreateBool(int innie)
{
	return (EXP_ValueHandle) new CBoolValue(innie!=0);
}



EXP_ValueHandle EXP_CreateString(const char* str)
{
	
	return (EXP_ValueHandle) new CStringValue(str,"");
}



void EXP_SetName(EXP_ValueHandle inval,const char* newname)
{
	((CValue*) inval)->SetName(newname);
}



/* calculate expression from inputtext */
EXP_ValueHandle EXP_ParseInput(const char* inputtext)
{
	CValue* resultval=NULL;
	CParser parser;
	CExpression* expr = parser.ProcessText(inputtext);
	if (expr)
	{
		resultval = expr->Calculate();
		expr->Release();
	}
	else
	{
		resultval = new CErrorValue("couldn't parsetext");
	}

	return (EXP_ValueHandle) resultval;
}



void EXP_ReleaseValue(EXP_ValueHandle inval)
{
	((CValue*) inval)->Release();
}



int EXP_IsValid(EXP_ValueHandle inval)
{
	return !((CValue*) inval)->IsError();
}



/* assign property 'propval' to 'destinationval' */
void EXP_SetProperty(EXP_ValueHandle destinationval,
					 const char* propname,
					 EXP_ValueHandle propval)
{
	((CValue*) destinationval)->SetProperty(propname,(CValue*)propval);
}



const char* EXP_GetText(EXP_ValueHandle inval)
{
	return ((CValue*) inval)->GetText();
}



EXP_ValueHandle EXP_GetProperty(EXP_ValueHandle inval,const char* propname)
{
	return (EXP_ValueHandle) ((CValue*)inval)->GetProperty(propname);
}
