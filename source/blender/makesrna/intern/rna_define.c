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

#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_genfile.h"
#include "DNA_sdna_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

/* Global used during defining */

BlenderDefRNA DefRNA;

/* Duplicated code since we can't link in blenkernel or blenlib */

#define MIN2(x,y) ((x)<(y)? (x): (y))
#define MAX2(x,y) ((x)>(y)? (x): (y))

void rna_addtail(ListBase *listbase, void *vlink)
{
	Link *link= vlink;

	link->next = NULL;
	link->prev = listbase->last;

	if (listbase->last) ((Link *)listbase->last)->next = link;
	if (listbase->first == 0) listbase->first = link;
	listbase->last = link;
}

void rna_freelistN(ListBase *listbase)
{
	Link *link, *next;
	
	for(link=listbase->first; link; link=next) {
		next= link->next;
		MEM_freeN(link);
	}
	
	listbase->first= listbase->last= NULL;
}

/* DNA utility function for looking up members */

typedef struct DNAStructMember {
	char *type;
	char *name;
	int arraylength;
} DNAStructMember;

static int rna_member_cmp(const char *name, const char *oname)
{
	int a=0;
	
	/* compare without pointer or array part */
	while(name[0]=='*')
		name++;
	while(oname[0]=='*')
		oname++;
	
	while(1) {
		if(name[a]=='[' && oname[a]==0) return 1;
		if(name[a]==0) break;
		if(name[a] != oname[a]) return 0;
		a++;
	}
	if(name[a]==0 && oname[a] == '.') return 2;
	if(name[a]==0 && oname[a] == '-' && oname[a+1] == '>') return 3;

	return (name[a] == oname[a]);
}

static int rna_find_sdna_member(SDNA *sdna, const char *structname, const char *membername, DNAStructMember *smember)
{
	char *dnaname;
	short *sp;
	int a, structnr, totmember, cmp;

	structnr= DNA_struct_find_nr(sdna, structname);
	if(structnr == -1)
		return 0;

	sp= sdna->structs[structnr];
	totmember= sp[1];
	sp+= 2;

	for(a=0; a<totmember; a++, sp+=2) {
		dnaname= sdna->names[sp[1]];

		cmp= rna_member_cmp(dnaname, membername);

		if(cmp == 1) {
			smember->type= sdna->types[sp[0]];
			smember->name= dnaname;
			smember->arraylength= DNA_elem_array_size(smember->name, strlen(smember->name));
			return 1;
		}
		else if(cmp == 2) {
			membername= strstr(membername, ".") + strlen(".");
			return rna_find_sdna_member(sdna, sdna->types[sp[0]], membername, smember);
		}
		else if(cmp == 3) {
			membername= strstr(membername, "->") + strlen("->");
			return rna_find_sdna_member(sdna, sdna->types[sp[0]], membername, smember);
		}
	}

	return 0;
}

/* Blender Data Definition */

BlenderRNA *RNA_create()
{
	BlenderRNA *brna;

	brna= MEM_callocN(sizeof(BlenderRNA), "BlenderRNA");

	DefRNA.sdna= DNA_sdna_from_data(DNAstr,  DNAlen, 0);
	DefRNA.structs.first= DefRNA.structs.last= NULL;
	DefRNA.error= 0;

	return brna;
}

void RNA_define_free(BlenderRNA *brna)
{
	StructDefRNA *strct;
	AllocDefRNA *alloc;

	for(alloc=DefRNA.allocs.first; alloc; alloc=alloc->next)
		MEM_freeN(alloc->mem);
	rna_freelistN(&DefRNA.allocs);

	for(strct=DefRNA.structs.first; strct; strct=strct->next)
		rna_freelistN(&strct->properties);

	rna_freelistN(&DefRNA.structs);

	if(DefRNA.sdna) {
		DNA_sdna_free(DefRNA.sdna);
		DefRNA.sdna= NULL;
	}

	DefRNA.error= 0;
}

void RNA_free(BlenderRNA *brna)
{
	StructRNA *strct;

	RNA_define_free(brna);

	for(strct=brna->structs.first; strct; strct=strct->next)
		rna_freelistN(&strct->properties);

	rna_freelistN(&brna->structs);
	
	MEM_freeN(brna);
}

/* Struct Definition */

StructRNA *RNA_def_struct(BlenderRNA *brna, const char *cname, const char *name)
{
	StructRNA *strct;
	StructDefRNA *ds;

	ds= MEM_callocN(sizeof(StructDefRNA), "StructDefRNA");
	rna_addtail(&DefRNA.structs, ds);

	strct= MEM_callocN(sizeof(StructRNA), "StructRNA");
	strct->cname= cname;
	strct->name= name;

	ds->strct= strct;

	rna_addtail(&brna->structs, strct);

	RNA_def_struct_sdna(strct, strct->cname);

	return strct;
}

void RNA_def_struct_sdna(StructRNA *srna, const char *structname)
{
	StructDefRNA *ds= DefRNA.structs.last;

	if(!DNA_struct_find_nr(DefRNA.sdna, structname)) {
		if(!DefRNA.silent) {
			fprintf(stderr, "RNA_def_struct_sdna: %s not found.\n", structname);
			DefRNA.error= 1;
		}
		return;
	}

	ds->dnaname= structname;
}

void RNA_def_struct_name_property(struct StructRNA *srna, struct PropertyRNA *prop)
{
	if(prop->type != PROP_STRING) {
		fprintf(stderr, "RNA_def_struct_name_property: must be a string property.\n");
		DefRNA.error= 1;
	}
	else
		srna->nameproperty= prop;
}

/* Property Definition */

PropertyRNA *RNA_def_property(StructRNA *strct, const char *cname, int type, int subtype)
{
	StructDefRNA *ds;
	PropertyDefRNA *dp;
	PropertyRNA *prop;

	ds= DefRNA.structs.last;
	dp= MEM_callocN(sizeof(PropertyDefRNA), "PropertyDefRNA");
	rna_addtail(&ds->properties, dp);

	switch(type) {
		case PROP_BOOLEAN:
			prop= MEM_callocN(sizeof(BooleanPropertyRNA), "BooleanPropertyRNA");
			break;
		case PROP_INT: {
			IntPropertyRNA *iprop;
			iprop= MEM_callocN(sizeof(IntPropertyRNA), "IntPropertyRNA");
			prop= &iprop->property;

			iprop->hardmin= (subtype == PROP_UNSIGNED)? 0: INT_MIN;
			iprop->hardmax= INT_MAX;

			iprop->softmin= (subtype == PROP_UNSIGNED)? 0: -10000; /* rather arbitrary .. */
			iprop->softmax= 10000;
			break;
		}
		case PROP_FLOAT: {
			FloatPropertyRNA *fprop;
			fprop= MEM_callocN(sizeof(FloatPropertyRNA), "FloatPropertyRNA");
			prop= &fprop->property;

			fprop->hardmin= (subtype == PROP_UNSIGNED)? 0: -FLT_MAX;
			fprop->hardmax= FLT_MAX;

			fprop->softmin= (subtype == PROP_UNSIGNED)? 0: -10000; /* rather arbitrary .. */
			fprop->softmax= 10000;
			break;
		}
		case PROP_STRING: {
			StringPropertyRNA *sprop;
			sprop= MEM_callocN(sizeof(StringPropertyRNA), "StringPropertyRNA");
			prop= &sprop->property;

			sprop->defaultvalue= "";
			sprop->maxlength= 0;
			break;
		}
		case PROP_ENUM:
			prop= MEM_callocN(sizeof(EnumPropertyRNA), "EnumPropertyRNA");
			break;
		case PROP_POINTER:
			prop= MEM_callocN(sizeof(PointerPropertyRNA), "PointerPropertyRNA");
			break;
		case PROP_COLLECTION:
			prop= MEM_callocN(sizeof(CollectionPropertyRNA), "CollectionPropertyRNA");
			break;
		default:
			fprintf(stderr, "RNA_def_property: invalid property type.\n");
			DefRNA.error= 1;
			return NULL;
	}

	dp->strct= strct;
	dp->prop= prop;

	prop->cname= cname;
	prop->type= type;
	prop->subtype= subtype;
	prop->name= cname;
	prop->description= "";
	prop->flag= PROP_EDITABLE|PROP_EVALUATEABLE;

	switch(type) {
		case PROP_BOOLEAN:
			DefRNA.silent= 1;
			RNA_def_property_boolean_sdna(prop, strct->cname, cname, 0);
			DefRNA.silent= 0;
			break;
		case PROP_INT: {
			DefRNA.silent= 1;
			RNA_def_property_int_sdna(prop, strct->cname, cname);
			DefRNA.silent= 0;
			break;
		}
		case PROP_FLOAT: {
			DefRNA.silent= 1;
			RNA_def_property_float_sdna(prop, strct->cname, cname);
			DefRNA.silent= 0;
			break;
		}
		case PROP_STRING: {
			DefRNA.silent= 1;
			RNA_def_property_string_sdna(prop, strct->cname, cname);
			DefRNA.silent= 0;
			break;
		}
		case PROP_ENUM:
			DefRNA.silent= 1;
			RNA_def_property_enum_sdna(prop, strct->cname, cname);
			DefRNA.silent= 0;
			break;
		case PROP_POINTER:
			DefRNA.silent= 1;
			RNA_def_property_pointer_sdna(prop, strct->cname, cname);
			DefRNA.silent= 0;
			break;
		case PROP_COLLECTION:
			DefRNA.silent= 1;
			RNA_def_property_collection_sdna(prop, strct->cname, cname);
			DefRNA.silent= 0;
			break;
	}

	rna_addtail(&strct->properties, prop);

	return prop;
}

void RNA_def_property_access(PropertyRNA *prop, int editable, int evaluatable)
{
	if(editable) prop->flag |= PROP_EDITABLE;
	else prop->flag &= ~PROP_EDITABLE;

	if(evaluatable) prop->flag |= PROP_EVALUATEABLE;
	else prop->flag &= ~PROP_EVALUATEABLE;
}

void RNA_def_property_array(PropertyRNA *prop, int arraylength)
{
	switch(prop->type) {
		case PROP_BOOLEAN:
		case PROP_INT:
		case PROP_FLOAT:
			prop->arraylength= arraylength;
			break;
		default:
			fprintf(stderr, "RNA_def_property_array: only boolean/int/float can be array.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_ui_text(PropertyRNA *prop, const char *name, const char *description)
{
	prop->name= name;
	prop->description= description;
}

void RNA_def_property_ui_range(PropertyRNA *prop, double min, double max, double step, double precision)
{
	switch(prop->type) {
		case PROP_INT: {
			IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
			iprop->softmin= (int)min;
			iprop->softmax= (int)max;
			iprop->step= (int)step;
			break;
		}
		case PROP_FLOAT: {
			FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
			fprop->softmin= (float)min;
			fprop->softmax= (float)max;
			fprop->step= (float)step;
			fprop->precision= (float)precision;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_ui_range: invalid type for ui range.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_range(PropertyRNA *prop, double min, double max)
{
	switch(prop->type) {
		case PROP_INT: {
			IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
			iprop->hardmin= (int)min;
			iprop->hardmax= (int)max;
			iprop->softmin= MAX2(min, iprop->hardmin);
			iprop->softmax= MIN2(max, iprop->hardmax);
			break;
		}
		case PROP_FLOAT: {
			FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
			fprop->hardmin= (float)min;
			fprop->hardmax= (float)max;
			fprop->softmin= MAX2(min, fprop->hardmin);
			fprop->softmax= MIN2(max, fprop->hardmax);
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_range: invalid type for range.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_struct_type(PropertyRNA *prop, const char *type)
{
	switch(prop->type) {
		case PROP_POINTER: {
			PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;
			pprop->structtype = (StructRNA*)type;
			break;
		}
		case PROP_COLLECTION: {
			CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;
			cprop->structtype = (StructRNA*)type;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_struct_type: invalid type for struct type.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_enum_items(PropertyRNA *prop, const PropertyEnumItem *item)
{
	int i;

	switch(prop->type) {
		case PROP_ENUM: {
			EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;
			eprop->item= item;
			eprop->totitem= 0;
			for(i=0; item[i].cname; i++)
				eprop->totitem++;

			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_struct_type: invalid type for struct type.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_string_maxlength(PropertyRNA *prop, int maxlength)
{
	switch(prop->type) {
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;
			sprop->maxlength= maxlength;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_string_maxlength: type is not string.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_boolean_default(PropertyRNA *prop, int value)
{
	switch(prop->type) {
		case PROP_BOOLEAN: {
			BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;
			bprop->defaultvalue= value;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_boolean_default: type is not boolean.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_boolean_array_default(PropertyRNA *prop, const int *array)
{
	switch(prop->type) {
		case PROP_BOOLEAN: {
			BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;
			bprop->defaultarray= array;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_boolean_default: type is not boolean.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_int_default(PropertyRNA *prop, int value)
{
	switch(prop->type) {
		case PROP_INT: {
			IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
			iprop->defaultvalue= value;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_int_default: type is not int.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_int_array_default(PropertyRNA *prop, const int *array)
{
	switch(prop->type) {
		case PROP_INT: {
			IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
			iprop->defaultarray= array;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_int_default: type is not int.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_float_default(PropertyRNA *prop, float value)
{
	switch(prop->type) {
		case PROP_FLOAT: {
			FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
			fprop->defaultvalue= value;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_float_default: type is not float.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_float_array_default(PropertyRNA *prop, const float *array)
{
	switch(prop->type) {
		case PROP_FLOAT: {
			FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
			fprop->defaultarray= array;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_float_default: type is not float.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_string_default(PropertyRNA *prop, const char *value)
{
	switch(prop->type) {
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;
			sprop->defaultvalue= value;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_string_default: type is not string.\n");
			DefRNA.error= 1;
			break;
	}
}

void RNA_def_property_enum_default(PropertyRNA *prop, int value)
{
	switch(prop->type) {
		case PROP_ENUM: {
			EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;
			eprop->defaultvalue= value;
			break;
		}
		default:
			fprintf(stderr, "RNA_def_property_enum_default: type is not enum.\n");
			DefRNA.error= 1;
			break;
	}
}

/* SDNA */

static PropertyDefRNA *rna_def_property_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
	DNAStructMember smember;
	StructDefRNA *ds= DefRNA.structs.last;
	PropertyDefRNA *dp= ds->properties.last;

	if(!structname)
		structname= ds->dnaname;
	if(!propname)
		propname= prop->cname;

	if(!rna_find_sdna_member(DefRNA.sdna, structname, propname, &smember)) {
		if(!DefRNA.silent) {
			fprintf(stderr, "rna_def_property_sdna: %s.%s not found.\n", structname, propname);
			DefRNA.error= 1;
		}
		return NULL;
	}

	if(smember.arraylength > 1)
		prop->arraylength= smember.arraylength;
	else
		prop->arraylength= 0;
	
	dp->dnastructname= structname;
	dp->dnaname= propname;
	dp->dnatype= smember.type;
	dp->dnaarraylength= smember.arraylength;

	return dp;
}

void RNA_def_property_boolean_sdna(PropertyRNA *prop, const char *structname, const char *propname, int bit)
{
	PropertyDefRNA *dp;
	
	if((dp=rna_def_property_sdna(prop, structname, propname)))
		dp->booleanbit= bit;
}

void RNA_def_property_int_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
	PropertyDefRNA *dp;
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
	
	if((dp= rna_def_property_sdna(prop, structname, propname))) {
		/* SDNA doesn't pass us unsigned unfortunately .. */
		if(strcmp(dp->dnatype, "char") == 0) {
			iprop->hardmin= iprop->softmin= CHAR_MIN;
			iprop->hardmax= iprop->softmax= CHAR_MAX;
		}
		else if(strcmp(dp->dnatype, "short") == 0) {
			iprop->hardmin= iprop->softmin= SHRT_MIN;
			iprop->hardmax= iprop->softmax= SHRT_MAX;
		}
		else if(strcmp(dp->dnatype, "int") == 0) {
			iprop->hardmin= INT_MIN;
			iprop->hardmax= INT_MAX;

			iprop->softmin= -10000; /* rather arbitrary .. */
			iprop->softmax= 10000;
		}

		if(prop->subtype == PROP_UNSIGNED)
			iprop->hardmin= iprop->softmin= 0;
	}
}

void RNA_def_property_float_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
	rna_def_property_sdna(prop, structname, propname);
}

void RNA_def_property_enum_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
	PropertyDefRNA *dp;
	
	if((dp=rna_def_property_sdna(prop, structname, propname))) {
		if(prop->arraylength) {
			prop->arraylength= 0;
			if(!DefRNA.silent) {
				fprintf(stderr, "RNA_def_property_enum_sdna: %s.%s, array not supported for enum type.\n", structname, propname);
				DefRNA.error= 1;
			}
		}
	}
}

void RNA_def_property_string_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
	PropertyDefRNA *dp;
	StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

	if((dp=rna_def_property_sdna(prop, structname, propname))) {
		if(prop->arraylength) {
			sprop->maxlength= prop->arraylength;
			prop->arraylength= 0;
		}
	}
}

void RNA_def_property_pointer_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
	PropertyDefRNA *dp;
	
	if((dp=rna_def_property_sdna(prop, structname, propname))) {
		if(prop->arraylength) {
			prop->arraylength= 0;
			if(!DefRNA.silent) {
				fprintf(stderr, "RNA_def_property_pointer_sdna: %s.%s, array not supported for pointer type.\n", structname, propname);
				DefRNA.error= 1;
			}
		}
	}
}

void RNA_def_property_collection_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
	PropertyDefRNA *dp;
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;
	
	if((dp=rna_def_property_sdna(prop, structname, propname))) {
		if(prop->arraylength) {
			prop->arraylength= 0;

			if(!DefRNA.silent) {
				fprintf(stderr, "RNA_def_property_collection_sdna: %s.%s, array not supported for collection type.\n", structname, propname);
				DefRNA.error= 1;
			}
		}

		if(strcmp(dp->dnatype, "ListBase") == 0) {
			cprop->next= (PropCollectionNextFunc)"rna_iterator_listbase_next";
			cprop->get= (PropCollectionGetFunc)"rna_iterator_listbase_get";
		}
	}
}

/* Functions */

void RNA_def_property_notify_func(PropertyRNA *prop, char *notify)
{
	if(notify) prop->notify= (PropNotifyFunc)notify;
}

void RNA_def_property_boolean_funcs(PropertyRNA *prop, char *get, char *set)
{
	BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

	if(prop->arraylength) {
		if(get) bprop->getarray= (PropBooleanArrayGetFunc)get;
		if(set) bprop->setarray= (PropBooleanArraySetFunc)set;
	}
	else {
		if(get) bprop->get= (PropBooleanGetFunc)get;
		if(set) bprop->set= (PropBooleanSetFunc)set;
	}
}

void RNA_def_property_int_funcs(PropertyRNA *prop, char *get, char *set)
{
	IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

	if(prop->arraylength) {
		if(get) iprop->getarray= (PropIntArrayGetFunc)get;
		if(set) iprop->setarray= (PropIntArraySetFunc)set;
	}
	else {
		if(get) iprop->get= (PropIntGetFunc)get;
		if(set) iprop->set= (PropIntSetFunc)set;
	}
}

void RNA_def_property_float_funcs(PropertyRNA *prop, char *get, char *set)
{
	FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

	if(prop->arraylength) {
		if(get) fprop->getarray= (PropFloatArrayGetFunc)get;
		if(set) fprop->setarray= (PropFloatArraySetFunc)set;
	}
	else {
		if(get) fprop->get= (PropFloatGetFunc)get;
		if(set) fprop->set= (PropFloatSetFunc)set;
	}
}

void RNA_def_property_enum_funcs(PropertyRNA *prop, char *get, char *set)
{
	EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;

	if(get) eprop->get= (PropEnumGetFunc)get;
	if(set) eprop->set= (PropEnumSetFunc)set;
}

void RNA_def_property_string_funcs(PropertyRNA *prop, char *get, char *length, char *set)
{
	StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

	if(get) sprop->get= (PropStringGetFunc)get;
	if(length) sprop->length= (PropStringLengthFunc)length;
	if(set) sprop->set= (PropStringSetFunc)set;
}

void RNA_def_property_pointer_funcs(PropertyRNA *prop, char *get, char *type, char *set)
{
	PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

	if(get) pprop->get= (PropPointerGetFunc)get;
	if(type) pprop->type= (PropPointerTypeFunc)type;
	if(set) pprop->set= (PropPointerSetFunc)set;
}

void RNA_def_property_collection_funcs(PropertyRNA *prop, char *begin, char *next, char *end, char *get, char *type, char *length, char *lookupint, char *lookupstring)
{
	CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

	if(begin) cprop->begin= (PropCollectionBeginFunc)begin;
	if(next) cprop->next= (PropCollectionNextFunc)next;
	if(end) cprop->end= (PropCollectionEndFunc)end;
	if(get) cprop->get= (PropCollectionGetFunc)get;
	if(type) cprop->type= (PropCollectionTypeFunc)type;
	if(length) cprop->length= (PropCollectionLengthFunc)length;
	if(lookupint) cprop->lookupint= (PropCollectionLookupIntFunc)lookupint;
	if(lookupstring) cprop->lookupstring= (PropCollectionLookupStringFunc)lookupstring;
}

