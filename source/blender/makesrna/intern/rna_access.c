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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

/* Pointer */

void RNA_pointer_main_get(struct Main *main, struct PointerRNA *r_ptr)
{
	r_ptr->data= main;
	r_ptr->type= &RNA_Main;
	r_ptr->id.data= NULL;
	r_ptr->id.type= NULL;
}

static void rna_pointer_inherit_id(PointerRNA *parent, PointerRNA *ptr)
{
	if(ptr->type && ptr->type->flag & STRUCT_ID) {
		ptr->id.data= ptr->data;
		ptr->id.type= ptr->type;
	}
	else {
		ptr->id.data= parent->id.data;
		ptr->id.type= parent->id.type;
	}
}

/* Property */

int RNA_property_editable(PropertyRNA *prop, PointerRNA *ptr)
{
	return (prop->flag & PROP_EDITABLE);
}

int RNA_property_evaluated(PropertyRNA *prop, PointerRNA *ptr)
{
	return (prop->flag & PROP_EVALUATED);
}

void RNA_property_notify(PropertyRNA *prop, struct bContext *C, PointerRNA *ptr)
{
	if(prop->notify)
		prop->notify(C, ptr);
}

int RNA_property_boolean_get(PropertyRNA *prop, PointerRNA *ptr)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	return bprop->get(ptr);
}

void RNA_property_boolean_set(PropertyRNA *prop, PointerRNA *ptr, int value)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	bprop->set(ptr, value);
}

int RNA_property_boolean_get_array(PropertyRNA *prop, PointerRNA *ptr, int index)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	return bprop->getarray(ptr, index);
}

void RNA_property_boolean_set_array(PropertyRNA *prop, PointerRNA *ptr, int index, int value)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	bprop->setarray(ptr, index, value);
}

int RNA_property_int_get(PropertyRNA *prop, PointerRNA *ptr)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	return iprop->get(ptr);
}

void RNA_property_int_set(PropertyRNA *prop, PointerRNA *ptr, int value)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	iprop->set(ptr, value);
}

int RNA_property_int_get_array(PropertyRNA *prop, PointerRNA *ptr, int index)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	return iprop->getarray(ptr, index);
}

void RNA_property_int_set_array(PropertyRNA *prop, PointerRNA *ptr, int index, int value)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	iprop->setarray(ptr, index, value);
}

float RNA_property_float_get(PropertyRNA *prop, PointerRNA *ptr)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	return fprop->get(ptr);
}

void RNA_property_float_set(PropertyRNA *prop, PointerRNA *ptr, float value)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	fprop->set(ptr, value);
}

float RNA_property_float_get_array(PropertyRNA *prop, PointerRNA *ptr, int index)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	return fprop->getarray(ptr, index);
}

void RNA_property_float_set_array(PropertyRNA *prop, PointerRNA *ptr, int index, float value)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	fprop->setarray(ptr, index, value);
}

void RNA_property_string_get(PropertyRNA *prop, PointerRNA *ptr, char *value)
{
	StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

	sprop->get(ptr, value);
}

char *RNA_property_string_get_alloc(PropertyRNA *prop, PointerRNA *ptr, char *fixedbuf, int fixedlen)
{
	char *buf;
	int length;

	length= RNA_property_string_length(prop, ptr);

	if(length+1 < fixedlen)
		buf= fixedbuf;
	else
		buf= MEM_callocN(sizeof(char)*(length+1), "RNA_string_get_alloc");

	RNA_property_string_get(prop, ptr, buf);

	return buf;
}

int RNA_property_string_length(PropertyRNA *prop, PointerRNA *ptr)
{
	StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

	return sprop->length(ptr);
}

void RNA_property_string_set(PropertyRNA *prop, PointerRNA *ptr, const char *value)
{
	StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

	sprop->set(ptr, value);
}

int RNA_property_enum_get(PropertyRNA *prop, PointerRNA *ptr)
{
	EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;

	return eprop->get(ptr);
}

void RNA_property_enum_set(PropertyRNA *prop, PointerRNA *ptr, int value)
{
	EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;

	eprop->set(ptr, value);
}

void RNA_property_pointer_get(PropertyRNA *prop, PointerRNA *ptr, PointerRNA *r_ptr)
{
	PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

	r_ptr->data= pprop->get(ptr);

	if(r_ptr->data) {
		r_ptr->type= RNA_property_pointer_type(prop, ptr);
		rna_pointer_inherit_id(ptr, r_ptr);
	}
	else
		memset(r_ptr, 0, sizeof(*r_ptr));
}

void RNA_property_pointer_set(PropertyRNA *prop, PointerRNA *ptr, PointerRNA *ptr_value)
{
	PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

	pprop->set(ptr, ptr_value->data);
}

StructRNA *RNA_property_pointer_type(PropertyRNA *prop, PointerRNA *ptr)
{
	PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

	if(pprop->type)
		return pprop->type(ptr);
	
	return pprop->structtype;
}

void RNA_property_collection_begin(PropertyRNA *prop, CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	iter->pointer= *ptr;
	cprop->begin(iter, ptr);
}

void RNA_property_collection_next(PropertyRNA *prop, CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	cprop->next(iter);
}

void RNA_property_collection_end(PropertyRNA *prop, CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(cprop->end)
		cprop->end(iter);
}

void RNA_property_collection_get(PropertyRNA *prop, CollectionPropertyIterator *iter, PointerRNA *r_ptr)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	r_ptr->data= cprop->get(iter);

	if(r_ptr->data) {
		r_ptr->type= RNA_property_collection_type(prop, iter);
		rna_pointer_inherit_id(&iter->pointer, r_ptr);
	}
	else
		memset(r_ptr, 0, sizeof(*r_ptr));
}

StructRNA *RNA_property_collection_type(PropertyRNA *prop, CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(cprop->type)
		return cprop->type(iter);
	
	return cprop->structtype;
}

int RNA_property_collection_length(PropertyRNA *prop, PointerRNA *ptr)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(cprop->length) {
		return cprop->length(ptr);
	}
	else {
		CollectionPropertyIterator iter;
		int length= 0;

		for(cprop->begin(&iter, ptr); iter.valid; cprop->next(&iter))
			length++;

		if(cprop->end)
			cprop->end(&iter);

		return length;
	}
}

int RNA_property_collection_lookup_int(PropertyRNA *prop, PointerRNA *ptr, int key, PointerRNA *r_ptr)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(cprop->lookupint) {
		/* we have a callback defined, use it */
		r_ptr->data= cprop->lookupint(ptr, key, &r_ptr->type);

		if(r_ptr->data) {
			if(!r_ptr->type)
				r_ptr->type= cprop->structtype;
			rna_pointer_inherit_id(ptr, r_ptr);

			return 1;
		}
		else {
			memset(r_ptr, 0, sizeof(*r_ptr));
			return 0;
		}
	}
	else {
		/* no callback defined, just iterate and find the nth item */
		CollectionPropertyIterator iter;
		int i;

		RNA_property_collection_begin(prop, &iter, ptr);
		for(i=0; iter.valid; RNA_property_collection_next(prop, &iter), i++) {
			if(i == key) {
				RNA_property_collection_get(prop, &iter, r_ptr);
				break;
			}
		}
		RNA_property_collection_end(prop, &iter);

		if(!iter.valid)
			memset(r_ptr, 0, sizeof(*r_ptr));

		return iter.valid;
	}
}

int RNA_property_collection_lookup_string(PropertyRNA *prop, PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(cprop->lookupstring) {
		/* we have a callback defined, use it */
		r_ptr->data= cprop->lookupstring(ptr, key, &r_ptr->type);

		if(r_ptr->data) {
			if(!r_ptr->type)
				r_ptr->type= cprop->structtype;
			rna_pointer_inherit_id(ptr, r_ptr);

			return 1;
		}
		else {
			memset(r_ptr, 0, sizeof(*r_ptr));
			return 0;
		}
	}
	else {
		/* no callback defined, compare with name properties if they exist */
		CollectionPropertyIterator iter;
		PropertyRNA *prop;
		PointerRNA iterptr;
		char name[256], *nameptr;
		int length, alloc, found= 0;

		RNA_property_collection_begin(prop, &iter, ptr);
		for(; iter.valid; RNA_property_collection_next(prop, &iter)) {
			RNA_property_collection_get(prop, &iter, &iterptr);

			if(iterptr.data && iterptr.type->nameproperty) {
				prop= iterptr.type->nameproperty;

				length= RNA_property_string_length(prop, &iterptr);

				if(sizeof(name)-1 < length) {
					nameptr= name;
					alloc= 0;
				}
				else {
					nameptr= MEM_mallocN(sizeof(char)*length+1, "RNA_lookup_string");
					alloc= 1;
				}

				RNA_property_string_get(prop, &iterptr, nameptr);

				if(strcmp(nameptr, key) == 0) {
					*r_ptr= iterptr;
					found= 1;
				}

				if(alloc)
					MEM_freeN(nameptr);

				if(found)
					break;
			}
		}
		RNA_property_collection_end(prop, &iter);

		if(!iter.valid)
			memset(r_ptr, 0, sizeof(*r_ptr));

		return iter.valid;
	}
}

/* Standard iterator functions */

void rna_iterator_listbase_begin(CollectionPropertyIterator *iter, ListBase *lb)
{
	iter->internal= lb->first;
	iter->valid= (iter->internal != NULL);
}

void rna_iterator_listbase_next(CollectionPropertyIterator *iter)
{
	iter->internal= ((Link*)iter->internal)->next;
	iter->valid= (iter->internal != NULL);
}

void *rna_iterator_listbase_get(CollectionPropertyIterator *iter)
{
	return iter->internal;
}

void rna_iterator_array_begin(CollectionPropertyIterator *iter, void *ptr, int itemsize, int length)
{
	ArrayIterator *internal;

	internal= MEM_callocN(sizeof(ArrayIterator), "ArrayIterator");
	internal->ptr= ptr;
	internal->endptr= ((char*)ptr)+length*itemsize;
	internal->itemsize= itemsize;

	iter->internal= internal;
	iter->valid= (internal->ptr != internal->endptr);
}

void rna_iterator_array_next(CollectionPropertyIterator *iter)
{
	ArrayIterator *internal= iter->internal;

	internal->ptr += internal->itemsize;
	iter->valid= (internal->ptr != internal->endptr);
}

void *rna_iterator_array_get(CollectionPropertyIterator *iter)
{
	ArrayIterator *internal= iter->internal;

	return internal->ptr;
}

void rna_iterator_array_end(CollectionPropertyIterator *iter)
{
	MEM_freeN(iter->internal);
}

/* RNA Path - Experiment */

static char *rna_path_token(const char **path, char *fixedbuf, int fixedlen, int bracket)
{
	const char *p;
	char *buf;
	int i, j, len, escape;

	len= 0;

	if(bracket) {
		/* get data between [], check escaping ] with \] */
		if(**path == '[') (*path)++;
		else return NULL;

		p= *path;

		escape= 0;
		while(*p && (*p != ']' || escape)) {
			escape= (*p == '\\');
			len++;
			p++;
		}

		if(*p != ']') return NULL;
	}
	else {
		/* get data until . or [ */
		p= *path;

		while(*p && *p != '.' && *p != '[') {
			len++;
			p++;
		}
	}
	
	/* empty, return */
	if(len == 0)
		return NULL;
	
	/* try to use fixed buffer if possible */
	if(len+1 < fixedlen)
		buf= fixedbuf;
	else
		buf= MEM_callocN(sizeof(char)*(len+1), "rna_path_token");

	/* copy string, taking into account escaped ] */
	for(p=*path, i=0, j=0; i<len; i++, p++) {
		if(*p == '\\' && *(p+1) == ']');
		else buf[j++]= *p;
	}

	buf[j]= 0;

	/* set path to start of next token */
	if(*p == ']') p++;
	if(*p == '.') p++;
	*path= p;

	return buf;
}

int RNA_path_resolve(PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
	PropertyRNA *prop;
	PointerRNA curptr, nextptr;
	char fixedbuf[256], *token;
	int len, intkey;

	prop= NULL;
	curptr= *ptr;

	while(*path) {
		/* look up property name in current struct */
		token= rna_path_token(&path, fixedbuf, sizeof(fixedbuf), 0);

		if(!token)
			return 0;

		for(prop=curptr.type->properties.first; prop; prop=prop->next)
			if(strcmp(token, prop->cname) == 0)
				break;

		if(token != fixedbuf)
			MEM_freeN(token);

		if(!prop)
			return 0;

		/* now look up the value of this property if it is a pointer or
		 * collection, otherwise return the property rna so that the
		 * caller can read the value of the property itself */
		if(prop->type == PROP_POINTER) {
			RNA_property_pointer_get(prop, &curptr, &nextptr);

			if(nextptr.data)
				curptr= nextptr;
			else
				return 0;
		}
		else if(prop->type == PROP_COLLECTION && *path) {
			/* resolve the lookup with [] brackets */
			token= rna_path_token(&path, fixedbuf, sizeof(fixedbuf), 1);

			if(!token)
				return 0;

			len= strlen(token);

			/* check for "" to see if it is a string */
			if(len >= 2 && *token == '"' && token[len-2] == '"') {
				/* strip away "" */
				token[len-2]= 0;
				RNA_property_collection_lookup_string(prop, &curptr, token+1, &nextptr);
			}
			else {
				/* otherwise do int lookup */
				intkey= atoi(token);
				RNA_property_collection_lookup_int(prop, &curptr, intkey, &nextptr);
			}

			if(token != fixedbuf)
				MEM_freeN(token);

			if(nextptr.data)
				curptr= nextptr;
			else
				return 0;
		}
	}

	*r_ptr= curptr;
	*r_prop= prop;

	return 1;
}

char *RNA_path_append(const char *path, PropertyRNA *prop, int intkey, const char *strkey)
{
	DynStr *dynstr;
	const char *s;
	char appendstr[128], *result;
	
	dynstr= BLI_dynstr_new();

	/* add .cname */
	if(path) {
		BLI_dynstr_append(dynstr, (char*)path);
		if(*path)
			BLI_dynstr_append(dynstr, ".");
	}

	BLI_dynstr_append(dynstr, (char*)prop->cname);

	if(prop->type == PROP_COLLECTION) {
		/* add ["strkey"] or [intkey] */
		BLI_dynstr_append(dynstr, "[");

		if(strkey) {
			BLI_dynstr_append(dynstr, "\"");
			for(s=strkey; *s; s++) {
				if(*s == '[') {
					appendstr[0]= '\\';
					appendstr[1]= *s;
					appendstr[2]= 0;
				}
				else {
					appendstr[0]= *s;
					appendstr[1]= 0;
				}
				BLI_dynstr_append(dynstr, appendstr);
			}
			BLI_dynstr_append(dynstr, "\"");
		}
		else {
			sprintf(appendstr, "%d", intkey);
			BLI_dynstr_append(dynstr, appendstr);
		}

		BLI_dynstr_append(dynstr, "]");
	}

	result= BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);

	return result;
}

char *RNA_path_back(const char *path)
{
	char fixedbuf[256];
	const char *previous, *current;
	char *result, *token;
	int i;

	if(!path)
		return NULL;

	previous= NULL;
	current= path;

	/* parse token by token until the end, then we back up to the previous
	 * position and strip of the next token to get the path one step back */
	while(*current) {
		token= rna_path_token(&current, fixedbuf, sizeof(fixedbuf), 0);

		if(!token)
			return NULL;
		if(token != fixedbuf)
			MEM_freeN(token);

		/* in case of collection we also need to strip off [] */
		token= rna_path_token(&current, fixedbuf, sizeof(fixedbuf), 1);
		if(token && token != fixedbuf)
			MEM_freeN(token);
		
		if(!*current)
			break;

		previous= current;
	}

	if(!previous)
		return NULL;

	/* copy and strip off last token */
	i= previous - path;
	result= BLI_strdup(path);

	if(i > 0 && result[i-1] == '.') i--;
	result[i]= 0;

	return result;
}

