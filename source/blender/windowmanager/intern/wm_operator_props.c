/**
* $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_ID.h"

#include "MEM_guardedalloc.h"

#include "BKE_idprop.h"

#include "WM_api.h"


/* wrapped to get property from a operator. */
static IDProperty *op_get_property(wmOperator *op, char *name)
{
	IDProperty *prop;
	
	if(!op->properties)
		return NULL;

	prop= IDP_GetPropertyFromGroup(op->properties, name);
	return prop;
}

/*
 * We need create a "group" to store the operator properties.
 * We don't have a WM_operator_new or some thing like that,
 * so this function is called by all the OP_set_* function
 * in case that op->properties is equal to NULL.
 */
static void op_init_property(wmOperator *op)
{
	IDPropertyTemplate val;
	val.i = 0; /* silence MSVC warning about uninitialized var when debugging */
	op->properties= IDP_New(IDP_GROUP, val, "property");
}

/* ***** Property API, exported ***** */
void OP_free_property(wmOperator *op)
{
	if(op->properties) {
	IDP_FreeProperty(op->properties);
	/*
	 * This need change, when the idprop code only
	 * need call IDP_FreeProperty. (check BKE_idprop.h)
	 */
	MEM_freeN(op->properties);
	op->properties= NULL;
	}
}

void OP_set_int(wmOperator *op, char *name, int value)
{
	IDPropertyTemplate val;
	IDProperty *prop;

	if(!op->properties)
		op_init_property(op);

	val.i= value;
	prop= IDP_New(IDP_INT, val, name);
	IDP_ReplaceInGroup(op->properties, prop);
}

void OP_set_float(wmOperator *op, char *name, float value)
{
	IDPropertyTemplate val;
	IDProperty *prop;

	if(!op->properties)
		op_init_property(op);

	val.f= value;
	prop= IDP_New(IDP_FLOAT, val, name);
	IDP_ReplaceInGroup(op->properties, prop);
}

void OP_set_int_array(wmOperator *op, char *name, int *array, short len)
{
	IDPropertyTemplate val;
	IDProperty *prop;
	short i;
	int *pointer;

	if(!op->properties)
		op_init_property(op);

	val.array.len= len;
	val.array.type= IDP_INT;
	prop= IDP_New(IDP_ARRAY, val, name);

	pointer= (int *)prop->data.pointer;
	for(i= 0; i < len; i++)
		pointer[i]= array[i];
	IDP_ReplaceInGroup(op->properties, prop);
}

void OP_set_float_array(wmOperator *op, char *name, float *array, short len)
{
	IDPropertyTemplate val;
	IDProperty *prop;
	short i;
	float *pointer;

	if(!op->properties)
		op_init_property(op);

	val.array.len= len;
	val.array.type= IDP_FLOAT;
	prop= IDP_New(IDP_ARRAY, val, name);

	pointer= (float *) prop->data.pointer;
	for(i= 0; i < len; i++)
		pointer[i]= array[i];
	IDP_ReplaceInGroup(op->properties, prop);
}

void OP_set_string(wmOperator *op, char *name, char *str)
{
	IDPropertyTemplate val;
	IDProperty *prop;

	if(!op->properties)
		op_init_property(op);

	val.str= str;
	prop= IDP_New(IDP_STRING, val, name);
	IDP_ReplaceInGroup(op->properties, prop);
}

int OP_get_int(wmOperator *op, char *name, int *value)
{
	IDProperty *prop= op_get_property(op, name);
	int status= 0;

	if ((prop) && (prop->type == IDP_INT)) {
		(*value)= prop->data.val;
		status= 1;
	}
	return (status);
}

int OP_get_float(wmOperator *op, char *name, float *value)
{
	IDProperty *prop= op_get_property(op, name);
	int status= 0;

	if ((prop) && (prop->type == IDP_FLOAT)) {
		(*value)= *(float*)&prop->data.val;
		status= 1;
	}
	return (status);
}

int OP_get_int_array(wmOperator *op, char *name, int *array, short *len)
{
	IDProperty *prop= op_get_property(op, name);
	short i;
	int status= 0;
	int *pointer;

	if ((prop) && (prop->type == IDP_ARRAY)) {
		pointer= (int *) prop->data.pointer;

		for(i= 0; (i < prop->len) && (i < *len); i++)
			array[i]= pointer[i];

		(*len)= i;
		status= 1;
	}
	return (status);
}

int OP_get_float_array(wmOperator *op, char *name, float *array, short *len)
{
	IDProperty *prop= op_get_property(op, name);
	short i;
	float *pointer;
	int status= 0;

	if ((prop) && (prop->type == IDP_ARRAY)) {
		pointer= (float *) prop->data.pointer;

		for(i= 0; (i < prop->len) && (i < *len); i++)
			array[i]= pointer[i];

		(*len)= i;
		status= 1;
	}
	return (status);
}

char *OP_get_string(wmOperator *op, char *name)
{
	IDProperty *prop= op_get_property(op, name);
	if ((prop) && (prop->type == IDP_STRING))
		return ((char *) prop->data.pointer);
	return (NULL);
}

void OP_verify_int(wmOperator *op, char *name, int value, int *result)
{
	int rvalue;

	if(OP_get_int(op, name, &rvalue))
		value= rvalue;
	else
		OP_set_int(op, name, value);

	if(result)
		*result= value;
}

void OP_verify_float(wmOperator *op, char *name, float value, int *result)
{
	float rvalue;

	if(OP_get_float(op, name, &rvalue))
		value= rvalue;
	else
		OP_set_float(op, name, value);
	
	if(result)
		*result= value;
}

char *OP_verify_string(wmOperator *op, char *name, char *str)
{
	char *result= OP_get_string(op, name);

	if(!result) {
		OP_set_string(op, name, str);
		result= OP_get_string(op, name);
	}

	return result;
}

void OP_verify_int_array(wmOperator *op, char *name, int *array, short len, int *resultarray, short *resultlen)
{
	int rarray[1];
	short rlen= 1;

	if(resultarray && resultlen) {
		if(!OP_get_int_array(op, name, resultarray, &rlen)) {
			OP_set_int_array(op, name, array, len);
			OP_get_int_array(op, name, resultarray, resultlen);
		}
	}
	else {
		if(!OP_get_int_array(op, name, rarray, &rlen))
			OP_set_int_array(op, name, array, len);
	}
}

void OP_verify_float_array(wmOperator *op, char *name, float *array, short len, float *resultarray, short *resultlen)
{
	float rarray[1];
	short rlen= 1;

	if(resultarray && resultlen) {
		if(!OP_get_float_array(op, name, resultarray, &rlen)) {
			OP_set_float_array(op, name, array, len);
			OP_get_float_array(op, name, resultarray, resultlen);
		}
	}
	else {
		if(!OP_get_float_array(op, name, rarray, &rlen))
			OP_set_float_array(op, name, array, len);
	}
}


