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

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#define RNA_VERSION_DATE "$Id$"

#ifdef _WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

/* Preprocessing */

static void rna_print_c_string(FILE *f, const char *str)
{
	static char *escape[] = {"\''", "\"\"", "\??", "\\\\","\aa", "\bb", "\ff", "\nn", "\rr", "\tt", "\vv", NULL};
	int i, j;

	fprintf(f, "\"");
	for(i=0; str[i]; i++) {
		for(j=0; escape[j]; j++)
			if(str[i] == escape[j][0])
				break;

		if(escape[j]) fprintf(f, "\\%c", escape[j][1]);
		else fprintf(f, "%c", str[i]);
	}
	fprintf(f, "\"");
}

static char *rna_alloc_function_name(const char *structname, const char *propname, const char *type)
{
	AllocDefRNA *alloc;
	char buffer[2048];
	char *result;

	snprintf(buffer, sizeof(buffer), "rna_%s_%s_%s", structname, propname, type);
	result= MEM_callocN(sizeof(char)*strlen(buffer)+1, "rna_alloc_function_name");
	strcpy(result, buffer);

	alloc= MEM_callocN(sizeof(AllocDefRNA), "AllocDefRNA");
	alloc->mem= result;
	rna_addtail(&DefRNA.allocs, alloc);

	return result;
}

static const char *rna_type_type(PropertyRNA *prop)
{
	switch(prop->type) {
		case PROP_BOOLEAN:
		case PROP_INT:
		case PROP_ENUM:
			return "int";
		case PROP_FLOAT:
			return "float";
		case PROP_STRING:
			return "char*";
		default:
			return "void*";
	}
}

static char *rna_def_property_get_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp)
{
	char *func;

	if(!dp->dnastructname || !dp->dnaname) {
		fprintf(stderr, "rna_def_property_get_func: %s.%s has no valid dna info.\n", srna->cname, prop->cname);
		DefRNA.error= 1;
		return NULL;
	}

	if(prop->type == PROP_STRING && ((StringPropertyRNA*)prop)->maxlength == 0) {
		fprintf(stderr, "rna_def_property_get_func: string %s.%s has max length 0.\n", srna->cname, prop->cname);
		DefRNA.error= 1;
		return NULL;
	}

	func= rna_alloc_function_name(srna->cname, prop->cname, "get");

	switch(prop->type) {
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;
			fprintf(f, "static void %s(PointerRNA *ptr, char *value)\n", func);
			fprintf(f, "{\n");
			fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
			fprintf(f, "	BLI_strncpy(value, data->%s, %d);\n", dp->dnaname, sprop->maxlength);
			fprintf(f, "}\n\n");
			break;
		}
		default:
			if(prop->arraylength) {
				fprintf(f, "static %s %s(PointerRNA *ptr, int index)\n", rna_type_type(prop), func);
				fprintf(f, "{\n");
				fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
				if(prop->type == PROP_BOOLEAN && dp->booleanbit && dp->dnaarraylength==1)
					fprintf(f, "	return ((data->%s & (%d<<index)) != 0);\n", dp->dnaname, dp->booleanbit);
				else if(prop->type == PROP_BOOLEAN && dp->booleanbit)
					fprintf(f, "	return ((data->%s[index] & %d) != 0);\n", dp->dnaname, dp->booleanbit);
				else
					fprintf(f, "	return (%s)(data->%s[index]);\n", rna_type_type(prop), dp->dnaname);
				fprintf(f, "}\n\n");
			}
			else {
				fprintf(f, "static %s %s(PointerRNA *ptr)\n", rna_type_type(prop), func);
				fprintf(f, "{\n");
				fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
				if(prop->type == PROP_BOOLEAN && dp->booleanbit)
					fprintf(f, "	return (((data->%s) & %d) != 0);\n", dp->dnaname, dp->booleanbit);
				else
					fprintf(f, "	return (%s)(data->%s);\n", rna_type_type(prop), dp->dnaname);
				fprintf(f, "}\n\n");
			}
			break;
	}

	return func;
}

static const char *rna_function_string(void *func)
{
	return (func)? (const char*)func: "NULL";
}

static void rna_float_print(FILE *f, float num)
{
	if(num == -FLT_MAX) fprintf(f, "-FLT_MAX");
	else if(num == FLT_MAX) fprintf(f, "FLT_MAX");
	else if((int)num == num) fprintf(f, "%.1ff", num);
	else fprintf(f, "%.10ff", num);
}

static void rna_clamp_value(FILE *f, PropertyRNA *prop)
{
	if(prop->type == PROP_INT) {
		IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

		if(iprop->hardmin != INT_MIN || iprop->hardmax != INT_MAX)
			fprintf(f, "	CLAMP(value, %d, %d);\n", iprop->hardmin, iprop->hardmax);
	}
	else if(prop->type == PROP_FLOAT) {
		FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

		if(fprop->hardmin != -FLT_MAX || fprop->hardmax != FLT_MAX) {
			fprintf(f, "	CLAMP(value, ");
			rna_float_print(f, fprop->hardmin); fprintf(f, ", ");
			rna_float_print(f, fprop->hardmax); fprintf(f, ");\n");
		}
	}
}

static char *rna_def_property_set_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp)
{
	char *func;

	if(!dp->dnastructname || !dp->dnaname) {
		fprintf(stderr, "rna_def_property_set_func: %s.%s has no valid dna info.\n", srna->cname, prop->cname);
		DefRNA.error= 1;
		return NULL;
	}

	func= rna_alloc_function_name(srna->cname, prop->cname, "set");

	switch(prop->type) {
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;
			fprintf(f, "static void %s(PointerRNA *ptr, const char *value)\n", func);
			fprintf(f, "{\n");
			fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
			fprintf(f, "	BLI_strncpy(data->%s, value, %d);\n", dp->dnaname, sprop->maxlength);
			fprintf(f, "}\n\n");
			break;
		}
		default:
			if(prop->arraylength) {
				fprintf(f, "static void %s(PointerRNA *ptr, int index, %s value)\n", func, rna_type_type(prop));
				fprintf(f, "{\n");
				fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
				if(prop->type == PROP_BOOLEAN && dp->booleanbit && dp->dnaarraylength==1) {
					fprintf(f, "	if(value) data->%s |= (%d<<index);\n", dp->dnaname, dp->booleanbit);
					fprintf(f, "	else data->%s &= ~(%d<<index);\n", dp->dnaname, dp->booleanbit);
				}
				else if(prop->type == PROP_BOOLEAN && dp->booleanbit) {
					fprintf(f, "	if(value) data->%s[index] |= %d;\n", dp->dnaname, dp->booleanbit);
					fprintf(f, "	else data->%s[index] &= ~%d;\n", dp->dnaname, dp->booleanbit);
				}
				else {
					rna_clamp_value(f, prop);
					fprintf(f, "	data->%s[index]= value;\n", dp->dnaname);
				}
				fprintf(f, "}\n\n");
			}
			else {
				fprintf(f, "static void %s(PointerRNA *ptr, %s value)\n", func, rna_type_type(prop));
				fprintf(f, "{\n");
				fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
				if(prop->type == PROP_BOOLEAN && dp->booleanbit) {
					fprintf(f, "	if(value) data->%s |= %d;\n", dp->dnaname, dp->booleanbit);
					fprintf(f, "	else data->%s &= ~%d;\n", dp->dnaname, dp->booleanbit);
				}
				else {
					rna_clamp_value(f, prop);
					fprintf(f, "	data->%s= value;\n", dp->dnaname);
				}
				fprintf(f, "}\n\n");
			}
			break;
	}

	return func;
}

static char *rna_def_property_length_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp)
{
	char *func= NULL;

	if(prop->type == PROP_STRING) {
		if(!dp->dnastructname || !dp->dnaname) {
			fprintf(stderr, "rna_def_property_length_func: %s.%s has no valid dna info.\n", srna->cname, prop->cname);
			DefRNA.error= 1;
			return NULL;
		}

		func= rna_alloc_function_name(srna->cname, prop->cname, "length");

		fprintf(f, "static int %s(PointerRNA *ptr)\n", func);
		fprintf(f, "{\n");
		fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
		fprintf(f, "	return strlen(data->%s);\n", dp->dnaname);
		fprintf(f, "}\n\n");
	}
	else if(prop->type == PROP_COLLECTION) {
		if(prop->type == PROP_COLLECTION && !dp->dnalengthname) {
			fprintf(stderr, "rna_def_property_length_func: %s.%s has no valid dna info.\n", srna->cname, prop->cname);
			DefRNA.error= 1;
			return NULL;
		}

		func= rna_alloc_function_name(srna->cname, prop->cname, "length");

		fprintf(f, "static int %s(PointerRNA *ptr)\n", func);
		fprintf(f, "{\n");
		fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
		fprintf(f, "	return data->%s;\n", dp->dnalengthname);
		fprintf(f, "}\n\n");
	}

	return func;
}

static char *rna_def_property_begin_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp)
{
	char *func;

	if(!dp->dnastructname || !dp->dnaname) {
		fprintf(stderr, "rna_def_property_begin_func: %s.%s has no valid dna info.\n", srna->cname, prop->cname);
		DefRNA.error= 1;
		return NULL;
	}

	func= rna_alloc_function_name(srna->cname, prop->cname, "begin");

	if(dp->dnalengthname) {
		fprintf(f, "static void %s(CollectionPropertyIterator *iter, PointerRNA *ptr)\n", func);
		fprintf(f, "{\n");
		fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
		fprintf(f, "	rna_iterator_array_begin(iter, data->%s, sizeof(data->%s[0]), data->%s);\n", dp->dnaname, dp->dnaname, dp->dnalengthname);
		fprintf(f, "}\n\n");
	}
	else {
		fprintf(f, "static void %s(CollectionPropertyIterator *iter, PointerRNA *ptr)\n", func);
		fprintf(f, "{\n");
		fprintf(f, "	%s *data= (%s*)ptr->data;\n", dp->dnastructname, dp->dnastructname);
		fprintf(f, "	rna_iterator_listbase_begin(iter, &data->%s);\n", dp->dnaname);
		fprintf(f, "}\n\n");
	}

	return func;
}

static void rna_def_property_funcs(FILE *f, PropertyDefRNA *dp)
{
	PropertyRNA *prop;
	StructRNA *srna;

	srna= dp->srna;
	prop= dp->prop;

	switch(prop->type) {
		case PROP_BOOLEAN: {
			BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;

			if(!prop->arraylength) {
				if(!bprop->get) bprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp);
				if(!bprop->set) bprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp);
			}
			else {
				if(!bprop->getarray) bprop->getarray= (void*)rna_def_property_get_func(f, srna, prop, dp);
				if(!bprop->setarray) bprop->setarray= (void*)rna_def_property_set_func(f, srna, prop, dp);
			}
			break;
		}
		case PROP_INT: {
			IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

			if(!prop->arraylength) {
				if(!iprop->get) iprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp);
				if(!iprop->set) iprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp);
			}
			else {
				if(!iprop->getarray) iprop->getarray= (void*)rna_def_property_get_func(f, srna, prop, dp);
				if(!iprop->setarray) iprop->setarray= (void*)rna_def_property_set_func(f, srna, prop, dp);
			}
			break;
		}
		case PROP_FLOAT: {
			FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

			if(!prop->arraylength) {
				if(!fprop->get) fprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp);
				if(!fprop->set) fprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp);
			}
			else {
				if(!fprop->getarray) fprop->getarray= (void*)rna_def_property_get_func(f, srna, prop, dp);
				if(!fprop->setarray) fprop->setarray= (void*)rna_def_property_set_func(f, srna, prop, dp);
			}
			break;
		}
		case PROP_ENUM: {
			EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;

			if(!eprop->get) eprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp);
			if(!eprop->set) eprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp);
			break;
		}
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

			if(!sprop->get) sprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp);
			if(!sprop->length) sprop->length= (void*)rna_def_property_length_func(f, srna, prop, dp);
			if(!sprop->set) sprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp);
			break;
		}
		case PROP_POINTER: {
			PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

			if(!pprop->get) pprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp);
			if(!pprop->set) pprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp);
			if(!pprop->structtype && !pprop->type) {
				fprintf(stderr, "rna_def_property_funcs: %s.%s, collection must have either type function or fixed type.\n", srna->cname, prop->cname);
				DefRNA.error= 1;
			}
			break;
		}
		case PROP_COLLECTION: {
			CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

			if(dp->dnatype && strcmp(dp->dnatype, "ListBase")==0) {
				if(!cprop->begin)
					cprop->begin= (void*)rna_def_property_begin_func(f, srna, prop, dp);
			}
			else if(dp->dnalengthname) {
				if(!cprop->begin)
					cprop->begin= (void*)rna_def_property_begin_func(f, srna, prop, dp);
				if(!cprop->length)
					cprop->length= (void*)rna_def_property_length_func(f, srna, prop, dp);
			}

			if(!cprop->begin) {
				fprintf(stderr, "rna_def_property_funcs: %s.%s, collection must have a begin function.\n", srna->cname, prop->cname);
				DefRNA.error= 1;
			}
			if(!cprop->next) {
				fprintf(stderr, "rna_def_property_funcs: %s.%s, collection must have a next function.\n", srna->cname, prop->cname);
				DefRNA.error= 1;
			}
			if(!cprop->get) {
				fprintf(stderr, "rna_def_property_funcs: %s.%s, collection must have a get function.\n", srna->cname, prop->cname);
				DefRNA.error= 1;
			}
			if(!cprop->structtype && !cprop->type) {
				fprintf(stderr, "rna_def_property_funcs: %s.%s, collection must have either type function or fixed type.\n", srna->cname, prop->cname);
				DefRNA.error= 1;
			}
			break;
		}
	}
}

static const char *rna_find_type(const char *type)
{
	StructDefRNA *ds;

	for(ds=DefRNA.structs.first; ds; ds=ds->next)
		if(ds->dnaname && strcmp(ds->dnaname, type)==0)
			return ds->srna->cname;
	
	return NULL;
}

static void rna_auto_types()
{
	StructDefRNA *ds;
	PropertyDefRNA *dp;

	for(ds=DefRNA.structs.first; ds; ds=ds->next) {
		for(dp=ds->properties.first; dp; dp=dp->next) {
			if(dp->dnatype) {
				if(dp->prop->type == PROP_POINTER) {
					PointerPropertyRNA *pprop= (PointerPropertyRNA*)dp->prop;

					if(!pprop->structtype && !pprop->type)
						pprop->structtype= (StructRNA*)rna_find_type(dp->dnatype);
				}
				else if(dp->prop->type== PROP_COLLECTION) {
					CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)dp->prop;

					if(!cprop->structtype && !cprop->type && strcmp(dp->dnatype, "ListBase")==0)
						cprop->structtype= (StructRNA*)rna_find_type(dp->dnatype);
				}
			}
		}
	}
}

static void rna_auto_functions(FILE *f)
{
	StructDefRNA *ds;
	PropertyDefRNA *dp;

	fprintf(f, "/* Autogenerated Functions */\n\n");

	for(ds=DefRNA.structs.first; ds; ds=ds->next)
		for(dp=ds->properties.first; dp; dp=dp->next)
			rna_def_property_funcs(f, dp);
}

static const char *rna_property_structname(PropertyType type)
{
	switch(type) {
		case PROP_BOOLEAN: return "BooleanPropertyRNA";
		case PROP_INT: return "IntPropertyRNA";
		case PROP_FLOAT: return "FloatPropertyRNA";
		case PROP_STRING: return "StringPropertyRNA";
		case PROP_ENUM: return "EnumPropertyRNA";
		case PROP_POINTER: return "PointerPropertyRNA";
		case PROP_COLLECTION: return "CollectionPropertyRNA";
		default: return "UnknownPropertyRNA";
	}
}

static const char *rna_property_typename(PropertyType type)
{
	switch(type) {
		case PROP_BOOLEAN: return "PROP_BOOLEAN";
		case PROP_INT: return "PROP_INT";
		case PROP_FLOAT: return "PROP_FLOAT";
		case PROP_STRING: return "PROP_STRING";
		case PROP_ENUM: return "PROP_ENUM";
		case PROP_POINTER: return "PROP_POINTER";
		case PROP_COLLECTION: return "PROP_COLLECTION";
		default: return "PROP_UNKNOWN";
	}
}

static const char *rna_property_subtypename(PropertyType type)
{
	switch(type) {
		case PROP_NONE: return "PROP_NONE";
		case PROP_UNSIGNED: return "PROP_UNSIGNED";
		case PROP_FILEPATH: return "PROP_FILEPATH";
		case PROP_COLOR: return "PROP_COLOR";
		case PROP_VECTOR: return "PROP_VECTOR";
		case PROP_MATRIX: return "PROP_MATRIX";
		default: return "PROP_UNKNOWN";
	}
}

static void rna_generate_prototypes(BlenderRNA *brna, FILE *f)
{
	StructRNA *srna;

	for(srna=brna->structs.first; srna; srna=srna->next)
		fprintf(f, "StructRNA RNA_%s;\n", srna->cname);
	fprintf(f, "\n");

	fprintf(f, "BlenderRNA BLENDER_RNA = {");

	srna= brna->structs.first;
	if(srna) fprintf(f, "{&RNA_%s, ", srna->cname);
	else fprintf(f, "{NULL, ");

	srna= brna->structs.last;
	if(srna) fprintf(f, "&RNA_%s}", srna->cname);
	else fprintf(f, "NULL}");

	fprintf(f, "};\n\n");
}

static void rna_generate_struct(BlenderRNA *brna, StructRNA *srna, FILE *f)
{
	PropertyRNA *prop;

	fprintf(f, "/* %s */\n", srna->name);

	if(srna->properties.first)
		fprintf(f, "\n");

	for(prop=srna->properties.first; prop; prop=prop->next)
		fprintf(f, "static %s rna_%s_%s;\n", rna_property_structname(prop->type), srna->cname, prop->cname);
	fprintf(f, "\n");

	for(prop=srna->properties.first; prop; prop=prop->next) {
		switch(prop->type) {
			case PROP_ENUM: {
				EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;
				int i;

				if(eprop->item) {
					fprintf(f, "static PropertyEnumItem rna_%s_%s_items[%d] = {", srna->cname, prop->cname, eprop->totitem);

					for(i=0; i<eprop->totitem; i++) {
						fprintf(f, "{%d, ", eprop->item[i].value);
						rna_print_c_string(f, eprop->item[i].cname); fprintf(f, ", ");
						rna_print_c_string(f, eprop->item[i].name); fprintf(f, "}");
						if(i != eprop->totitem-1)
							fprintf(f, ", ");
					}

					fprintf(f, "};\n\n");
				}
				break;
			}
			case PROP_BOOLEAN: {
				BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;
				unsigned int i;

				if(bprop->defaultarray) {
					fprintf(f, "static int rna_%s_%s_default[%d] = {", srna->cname, prop->cname, prop->arraylength);

					for(i=0; i<prop->arraylength; i++) {
						fprintf(f, "%d", bprop->defaultarray[i]);
						if(i != prop->arraylength-1)
							fprintf(f, ", ");
					}

					fprintf(f, "};\n\n");
				}
				break;
			}
			case PROP_INT: {
				IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
				unsigned int i;

				if(iprop->defaultarray) {
					fprintf(f, "static int rna_%s_%s_default[%d] = {", srna->cname, prop->cname, prop->arraylength);

					for(i=0; i<prop->arraylength; i++) {
						fprintf(f, "%d", iprop->defaultarray[i]);
						if(i != prop->arraylength-1)
							fprintf(f, ", ");
					}

					fprintf(f, "};\n\n");
				}
				break;
			}
			case PROP_FLOAT: {
				FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
				unsigned int i;

				if(fprop->defaultarray) {
					fprintf(f, "static float rna_%s_%s_default[%d] = {", srna->cname, prop->cname, prop->arraylength);

					for(i=0; i<prop->arraylength; i++) {
						rna_float_print(f, fprop->defaultarray[i]);
						if(i != prop->arraylength-1)
							fprintf(f, ", ");
					}

					fprintf(f, "};\n\n");
				}
				break;
			}
			default:
				break;
		}

		fprintf(f, "static %s rna_%s_%s = {\n", rna_property_structname(prop->type), srna->cname, prop->cname);

		if(prop->next) fprintf(f, "\t{(PropertyRNA*)&rna_%s_%s, ", srna->cname, prop->next->cname);
		else fprintf(f, "\t{NULL, ");
		if(prop->prev) fprintf(f, "(PropertyRNA*)&rna_%s_%s,\n", srna->cname, prop->prev->cname);
		else fprintf(f, "NULL,\n");
		fprintf(f, "\t"); rna_print_c_string(f, prop->cname);
		fprintf(f, ", %d, ", prop->flag);
		rna_print_c_string(f, prop->name); fprintf(f, ",\n\t");
		rna_print_c_string(f, prop->description); fprintf(f, ",\n");
		fprintf(f, "\t%s, %s, %d,\n", rna_property_typename(prop->type), rna_property_subtypename(prop->subtype), prop->arraylength);
		fprintf(f, "\t%s},\n", rna_function_string(prop->notify));

		switch(prop->type) {
			case PROP_BOOLEAN: {
				BooleanPropertyRNA *bprop= (BooleanPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s, %d, ", rna_function_string(bprop->get), rna_function_string(bprop->set), rna_function_string(bprop->getarray), rna_function_string(bprop->setarray), bprop->defaultvalue);
				if(bprop->defaultarray) fprintf(f, "rna_%s_%s_default\n", srna->name, prop->cname);
				else fprintf(f, "NULL\n");
				break;
			}
			case PROP_INT: {
				IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s, %d, %d, %d, %d, %d,\n\t%d, \n", rna_function_string(iprop->get), rna_function_string(iprop->set), rna_function_string(iprop->getarray), rna_function_string(iprop->setarray), iprop->softmin, iprop->softmax, iprop->hardmin, iprop->hardmax, iprop->step, iprop->defaultvalue);
				if(iprop->defaultarray) fprintf(f, "rna_%s_%s_default\n", srna->name, prop->cname);
				else fprintf(f, "NULL\n");
				break;
			}
			case PROP_FLOAT: {
				FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s, ", rna_function_string(fprop->get), rna_function_string(fprop->set), rna_function_string(fprop->getarray), rna_function_string(fprop->setarray));
				rna_float_print(f, fprop->softmin); fprintf(f, ", ");
				rna_float_print(f, fprop->softmax); fprintf(f, ", ");
				rna_float_print(f, fprop->hardmin); fprintf(f, ", ");
				rna_float_print(f, fprop->hardmax); fprintf(f, ", ");
				rna_float_print(f, fprop->step); fprintf(f, ", ");
				rna_float_print(f, fprop->precision); fprintf(f, ", ");
				rna_float_print(f, fprop->defaultvalue); fprintf(f, ", ");
				if(fprop->defaultarray) fprintf(f, "rna_%s_%s_default\n", srna->name, prop->cname);
				else fprintf(f, "NULL\n");
				break;
			}
			case PROP_STRING: {
				StringPropertyRNA *sprop= (StringPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %d, ", rna_function_string(sprop->get), rna_function_string(sprop->length), rna_function_string(sprop->set), sprop->maxlength);
				rna_print_c_string(f, sprop->defaultvalue); fprintf(f, "\n");
				break;
			}
			case PROP_ENUM: {
				EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, rna_%s_%s_items, %d, %d\n", rna_function_string(eprop->get), rna_function_string(eprop->set), srna->cname, prop->cname, eprop->totitem, eprop->defaultvalue);
				break;
			}
			case PROP_POINTER: {
				PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, ", rna_function_string(pprop->get), rna_function_string(pprop->set), rna_function_string(pprop->type));
				if(pprop->structtype) fprintf(f, "&RNA_%s\n", (char*)pprop->structtype);
				else fprintf(f, "NULL\n");
				break;
			}
			case PROP_COLLECTION: {
				CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s, %s, %s, %s, %s, ", rna_function_string(cprop->begin), rna_function_string(cprop->next), rna_function_string(cprop->end), rna_function_string(cprop->get), rna_function_string(cprop->type), rna_function_string(cprop->length), rna_function_string(cprop->lookupint), rna_function_string(cprop->lookupstring));
				if(cprop->structtype) fprintf(f, "&RNA_%s\n", (char*)cprop->structtype);
				else fprintf(f, "NULL\n");
				break;
			}
		}

		fprintf(f, "};\n\n");
	}

	fprintf(f, "StructRNA RNA_%s = {\n", srna->cname);

	if(srna->next) fprintf(f, "\t&RNA_%s, ", srna->next->cname);
	else fprintf(f, "\tNULL, ");
	if(srna->prev) fprintf(f, "&RNA_%s,\n", srna->prev->cname);
	else fprintf(f, "NULL,\n");

	fprintf(f, "\t");
	rna_print_c_string(f, srna->cname);
	fprintf(f, ", %d, ", srna->flag);
	rna_print_c_string(f, srna->name);
	fprintf(f, ",\n");

	prop= srna->nameproperty;
	if(prop) fprintf(f, "\t(PropertyRNA*)&rna_%s_%s,\n", srna->cname, prop->cname);
	else fprintf(f, "\tNULL,\n");

	prop= srna->properties.first;
	if(prop) fprintf(f, "\t{(PropertyRNA*)&rna_%s_%s, ", srna->cname, prop->cname);
	else fprintf(f, "\t{NULL, ");

	prop= srna->properties.last;
	if(prop) fprintf(f, "(PropertyRNA*)&rna_%s_%s}\n", srna->cname, prop->cname);
	else fprintf(f, "NULL}\n");

	fprintf(f, "};\n");

	fprintf(f, "\n");
}

typedef struct RNAProcessItem {
	char *filename;
	void (*define)(BlenderRNA *brna);
} RNAProcessItem;

RNAProcessItem PROCESS_ITEMS[]= {
	{"rna_ID.c", NULL},
	{"rna_main.c", RNA_def_main},
	{"rna_mesh.c", RNA_def_mesh},
	{"rna_object.c", RNA_def_object},
	{"rna_scene.c", RNA_def_scene},
	{NULL, NULL}};

static int rna_preprocess(char *basedirectory, FILE *f)
{
	BlenderRNA *brna;
	StructRNA *srna;
	int i, status;
	
	fprintf(f, "\n/* Automatically generated struct definitions for the Data API.\n"
	                "   Do not edit manually, changes will be overwritten */\n\n"
	                "#define RNA_RUNTIME\n\n");

	brna= RNA_create();

	fprintf(f, "#include <float.h>\n");
	fprintf(f, "#include <limits.h>\n");
	fprintf(f, "#include <string.h>\n\n");

	fprintf(f, "#include \"BLI_blenlib.h\"\n\n");

	fprintf(f, "#include \"BKE_utildefines.h\"\n\n");

	fprintf(f, "#include \"RNA_define.h\"\n\n");
	fprintf(f, "#include \"RNA_types.h\"\n\n");

	fprintf(f, "#include \"rna_internal.h\"\n\n");
	for(i=0; PROCESS_ITEMS[i].filename; i++)
		fprintf(f, "#include \"%s\"\n", PROCESS_ITEMS[i].filename);
	fprintf(f, "\n");

	for(i=0; PROCESS_ITEMS[i].filename; i++)
		if(PROCESS_ITEMS[i].define)
			PROCESS_ITEMS[i].define(brna);
	rna_auto_types();
	
	rna_generate_prototypes(brna, f);
	rna_auto_functions(f);

	for(srna=brna->structs.first; srna; srna=srna->next)
		rna_generate_struct(brna, srna, f);
	
	status= DefRNA.error;

	RNA_define_free(brna);
	RNA_free(brna);

	return status;
}

static void make_bad_file(char *file)
{
	FILE *fp= fopen(file, "w");
	fprintf(fp, "ERROR! Cannot make correct RNA.c file, STUPID!\n");
	fclose(fp);
}

#ifndef BASE_HEADER
#define BASE_HEADER "../"
#endif

int main(int argc, char **argv)
{
	FILE *file;
	int return_status = 0;
	extern int totblock;

	if (argc!=2 && argc!=3) {
		printf("Usage: %s outfile.c [base directory]\n", argv[0]);
		return_status = 1;
	}
	else {
		file = fopen(argv[1], "w");

		if (!file) {
			printf ("Unable to open file: %s\n", argv[1]);
			return_status = 1;
		}
		else {
			char baseDirectory[256];

			printf("Running makesrna, program versions %s\n",  RNA_VERSION_DATE);

			if (argc==3)
				strcpy(baseDirectory, argv[2]);
			else
				strcpy(baseDirectory, BASE_HEADER);

			return_status= (rna_preprocess(baseDirectory, file));
			fclose(file);

			if(return_status) {
				/* error */
				make_bad_file(argv[1]);
				return_status = 1;
			}
		}
	}

	if(totblock!=0) {
		printf("Error Totblock: %d\n",totblock);
		MEM_printmemlist();
	}

	return return_status;
}

