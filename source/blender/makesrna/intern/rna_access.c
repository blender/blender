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

/* Accessors */

void RNA_property_notify(PropertyRNA *prop, struct bContext *C, void *data)
{
	if(prop->notify)
		prop->notify(C, data);
}

int RNA_property_editable(PropertyRNA *prop, struct bContext *C, void *data)
{
	return (prop->flag & PROP_EDITABLE);
}

int RNA_property_evaluatable(PropertyRNA *prop, struct bContext *C, void *data)
{
	return (prop->flag & PROP_EVALUATEABLE);
}

int RNA_property_boolean_get(PropertyRNA *prop, void *data)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	return bprop->get(data);
}

void RNA_property_boolean_set(PropertyRNA *prop, void *data, int value)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	bprop->set(data, value);
}

int RNA_property_boolean_get_array(PropertyRNA *prop, void *data, int index)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	return bprop->getarray(data, index);
}

void RNA_property_boolean_set_array(PropertyRNA *prop, void *data, int index, int value)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	bprop->setarray(data, index, value);
}

int RNA_property_int_get(PropertyRNA *prop, void *data)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	return iprop->get(data);
}

void RNA_property_int_set(PropertyRNA *prop, void *data, int value)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	iprop->set(data, value);
}

int RNA_property_int_get_array(PropertyRNA *prop, void *data, int index)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	return iprop->getarray(data, index);
}

void RNA_property_int_set_array(PropertyRNA *prop, void *data, int index, int value)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	iprop->setarray(data, index, value);
}

float RNA_property_float_get(PropertyRNA *prop, void *data)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	return fprop->get(data);
}

void RNA_property_float_set(PropertyRNA *prop, void *data, float value)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	fprop->set(data, value);
}

float RNA_property_float_get_array(PropertyRNA *prop, void *data, int index)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	return fprop->getarray(data, index);
}

void RNA_property_float_set_array(PropertyRNA *prop, void *data, int index, float value)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	fprop->setarray(data, index, value);
}

void RNA_property_string_get(PropertyRNA *prop, void *data, char *value)
{
	StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

	sprop->get(data, value);
}

int RNA_property_string_length(PropertyRNA *prop, void *data)
{
	StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

	return sprop->length(data);
}

void RNA_property_string_set(PropertyRNA *prop, void *data, const char *value)
{
	StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

	sprop->set(data, value);
}

int RNA_property_enum_get(PropertyRNA *prop, void *data)
{
	EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;

	return eprop->get(data);
}

void RNA_property_enum_set(PropertyRNA *prop, void *data, int value)
{
	EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;

	eprop->set(data, value);
}

void *RNA_property_pointer_get(PropertyRNA *prop, void *data)
{
	PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

	return pprop->get(data);
}

void RNA_property_pointer_set(PropertyRNA *prop, void *data, void *value)
{
	PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

	pprop->set(data, value);
}

StructRNA *RNA_property_pointer_type(PropertyRNA *prop, void *data)
{
	PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

	if(pprop->type)
		return pprop->type(data);
	
	return pprop->structtype;
}

void RNA_property_collection_begin(PropertyRNA *prop, struct CollectionPropertyIterator *iter, void *data)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	cprop->begin(iter, data);
}

void RNA_property_collection_next(PropertyRNA *prop, struct CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	cprop->next(iter);
}

void RNA_property_collection_end(PropertyRNA *prop, struct CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(cprop->end)
		cprop->end(iter);
}

void *RNA_property_collection_get(PropertyRNA *prop, struct CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	return cprop->get(iter);
}

StructRNA *RNA_property_collection_type(PropertyRNA *prop, struct CollectionPropertyIterator *iter)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(cprop->type)
		return cprop->type(iter);
	
	return cprop->structtype;
}

int RNA_property_collection_length(PropertyRNA *prop, void *data)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(cprop->length) {
		return cprop->length(data);
	}
	else {
		CollectionPropertyIterator iter;
		int length= 0;

		for(cprop->begin(&iter, data); iter.valid; cprop->next(&iter))
			length++;

		if(cprop->end)
			cprop->end(&iter);

		return length;
	}
}

void *RNA_property_collection_lookup_int(PropertyRNA *prop, void *data, int key, StructRNA **type)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;
	void *value;

	if(cprop->lookupint) {
		value= cprop->lookupint(data, key, type);

		if(value && type && !*type)
			*type= cprop->structtype;

		return value;
	}
	else {
		CollectionPropertyIterator iter;
		int i= 0;
		void *value;

		for(cprop->begin(&iter, data); iter.valid; cprop->next(&iter), i++)
			if(i == key)
				break;

		if(iter.valid) {
			value= cprop->get(&iter);
			if(type) *type= RNA_property_collection_type(prop, &iter);
		}
		else {
			value= NULL;
			if(type) *type= NULL;
		}

		if(cprop->end)
			cprop->end(&iter);

		return value;
	}
}

void *RNA_property_collection_lookup_string(PropertyRNA *prop, void *data, const char *key, StructRNA **type)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;
	void *value;

	if(cprop->lookupstring) {
		value= cprop->lookupstring(data, key, type);

		if(value && type && !*type)
			*type= cprop->structtype;

		return value;
	}
	else {
		CollectionPropertyIterator iter;
		StructRNA *itertype= NULL;
		StringPropertyRNA *sprop;
		void *value= NULL;
		char name[256], *nameptr;
		int length, alloc, found= 0;

		for(cprop->begin(&iter, data); iter.valid && !found; cprop->next(&iter)) {
			itertype= RNA_property_collection_type(prop, &iter);

			if(itertype->nameproperty) {
				value= cprop->get(&iter);

				if(value) {
					sprop= (StringPropertyRNA*)itertype->nameproperty;
					length= sprop->length(value);

					if(sizeof(name)-1 < length) {
						nameptr= name;
						alloc= 0;
					}
					else {
						nameptr= MEM_mallocN(sizeof(char)*length+1, "RNA_lookup_string");
						alloc= 1;
					}

					sprop->get(value, nameptr);

					if(strcmp(nameptr, key) == 0)
						found= 1;

					if(alloc)
						MEM_freeN(nameptr);
				}
			}
		}

		if(found && type)
			*type= itertype;

		if(cprop->end)
			cprop->end(&iter);

		return value;
	}
	
	return NULL;
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

typedef struct ArrayIterator {
	char *ptr;
	char *endptr;
	int itemsize;
} ArrayIterator;

void rna_iterator_array_begin(CollectionPropertyIterator *iter, void *ptr, int itemsize, int length)
{
	ArrayIterator *internal;

	internal= MEM_callocN(sizeof(ArrayIterator), "ArrayIterator");
	internal->ptr= ptr;
	internal->endptr= ((char*)ptr)+length*itemsize;

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

