/*
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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/makesrna.c
 *  \ingroup RNA
 */


#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#define RNA_VERSION_DATE "FIXME-RNA_VERSION_DATE"

#ifdef _WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

/* Replace if different */
#define TMP_EXT ".tmp"


/* copied from BLI_file_older */
#include <sys/stat.h>
static int file_older(const char *file1, const char *file2)
{
	struct stat st1, st2;
	// printf("compare: %s %s\n", file1, file2);

	if(stat(file1, &st1)) return 0;
	if(stat(file2, &st2)) return 0;

	return (st1.st_mtime < st2.st_mtime);
}
static const char *makesrna_path= NULL;

static int replace_if_different(char *tmpfile, const char *dep_files[])
{
	// return 0; // use for testing had edited rna

#define REN_IF_DIFF                                                           \
	{                                                                         \
		FILE *file_test= fopen(orgfile, "rb");                                \
		if(file_test) {                                                       \
			fclose(file_test);                                                \
			if(fp_org) fclose(fp_org);                                        \
			if(fp_new) fclose(fp_new);                                        \
			if(remove(orgfile) != 0) {                                        \
				fprintf(stderr, "%s:%d, Remove Error (%s): \"%s\"\n",         \
				        __FILE__, __LINE__, strerror(errno), orgfile);        \
				return -1;                                                    \
			}                                                                 \
		}                                                                     \
	}                                                                         \
	if(rename(tmpfile, orgfile) != 0) {                                       \
		fprintf(stderr, "%s:%d, Rename Error (%s): \"%s\" -> \"%s\"\n",       \
		        __FILE__, __LINE__, strerror(errno), tmpfile, orgfile);       \
		return -1;                                                            \
	}                                                                         \
	remove(tmpfile);                                                          \
	return 1;                                                                 \

/* end REN_IF_DIFF */


	FILE *fp_new= NULL, *fp_org= NULL;
	int len_new, len_org;
	char *arr_new, *arr_org;
	int cmp;

	char orgfile[4096];

	strcpy(orgfile, tmpfile);
	orgfile[strlen(orgfile) - strlen(TMP_EXT)] = '\0'; /* strip '.tmp' */

	fp_org= fopen(orgfile, "rb");

	if(fp_org==NULL) {
		REN_IF_DIFF;
	}


	/* XXX, trick to work around dependancy problem
	 * assumes dep_files is in the same dir as makesrna.c, which is true for now. */

	if(1) {
		/* first check if makesrna.c is newer then generated files
		 * for development on makesrna.c you may want to disable this */
		if(file_older(orgfile, __FILE__)) {
			REN_IF_DIFF;
		}

		if(file_older(orgfile, makesrna_path)) {
			REN_IF_DIFF;
		}

		/* now check if any files we depend on are newer then any generated files */
		if(dep_files) {
			int pass;
			for(pass=0; dep_files[pass]; pass++) {
				char from_path[4096]= __FILE__;
				char *p1, *p2;

				/* dir only */
				p1= strrchr(from_path, '/');
				p2= strrchr(from_path, '\\');
				strcpy((p1 > p2 ? p1 : p2)+1, dep_files[pass]);
				/* account for build deps, if makesrna.c (this file) is newer */
				if(file_older(orgfile, from_path)) {
					REN_IF_DIFF;
				}
			}
		}
	}
	/* XXX end dep trick */


	fp_new= fopen(tmpfile, "rb");

	if(fp_new==NULL) {
		/* shouldn't happen, just to be safe */
		fprintf(stderr, "%s:%d, open error: \"%s\"\n", __FILE__, __LINE__, tmpfile);
		fclose(fp_org);
		return -1;
	}

	fseek(fp_new, 0L, SEEK_END); len_new = ftell(fp_new); fseek(fp_new, 0L, SEEK_SET);
	fseek(fp_org, 0L, SEEK_END); len_org = ftell(fp_org); fseek(fp_org, 0L, SEEK_SET);


	if(len_new != len_org) {
		fclose(fp_new);
		fclose(fp_org);
		REN_IF_DIFF;
	}

	/* now compare the files... */
	arr_new= MEM_mallocN(sizeof(char)*len_new, "rna_cmp_file_new");
	arr_org= MEM_mallocN(sizeof(char)*len_org, "rna_cmp_file_org");

	if(fread(arr_new, sizeof(char), len_new, fp_new) != len_new)
		fprintf(stderr, "%s:%d, error reading file %s for comparison.\n", __FILE__, __LINE__, tmpfile);
	if(fread(arr_org, sizeof(char), len_org, fp_org) != len_org)
		fprintf(stderr, "%s:%d, error reading file %s for comparison.\n", __FILE__, __LINE__, orgfile);

	fclose(fp_new);
	fclose(fp_org);

	cmp= memcmp(arr_new, arr_org, len_new);

	MEM_freeN(arr_new);
	MEM_freeN(arr_org);

	if(cmp) {
		REN_IF_DIFF;
	}
	else {
		remove(tmpfile);
		return 0;
	}

#undef REN_IF_DIFF
}

/* Helper to solve keyword problems with C/C++ */

static const char *rna_safe_id(const char *id)
{
	if(strcmp(id, "default") == 0)
		return "default_value";
	else if(strcmp(id, "operator") == 0)
		return "operator_value";

	return id;
}

/* Sorting */

static int cmp_struct(const void *a, const void *b)
{
	const StructRNA *structa= *(const StructRNA**)a;
	const StructRNA *structb= *(const StructRNA**)b;

	return strcmp(structa->identifier, structb->identifier);
}

static int cmp_property(const void *a, const void *b)
{
	const PropertyRNA *propa= *(const PropertyRNA**)a;
	const PropertyRNA *propb= *(const PropertyRNA**)b;

	if(strcmp(propa->identifier, "rna_type") == 0) return -1;
	else if(strcmp(propb->identifier, "rna_type") == 0) return 1;

	if(strcmp(propa->identifier, "name") == 0) return -1;
	else if(strcmp(propb->identifier, "name") == 0) return 1;

	return strcmp(propa->name, propb->name);
}

static int cmp_def_struct(const void *a, const void *b)
{
	const StructDefRNA *dsa= *(const StructDefRNA**)a;
	const StructDefRNA *dsb= *(const StructDefRNA**)b;

	return cmp_struct(&dsa->srna, &dsb->srna);
}

static int cmp_def_property(const void *a, const void *b)
{
	const PropertyDefRNA *dpa= *(const PropertyDefRNA**)a;
	const PropertyDefRNA *dpb= *(const PropertyDefRNA**)b;

	return cmp_property(&dpa->prop, &dpb->prop);
}

static void rna_sortlist(ListBase *listbase, int(*cmp)(const void*, const void*))
{
	Link *link;
	void **array;
	int a, size;
	
	if(listbase->first == listbase->last)
		return;

	for(size=0, link=listbase->first; link; link=link->next)
		size++;

	array= MEM_mallocN(sizeof(void*)*size, "rna_sortlist");
	for(a=0, link=listbase->first; link; link=link->next, a++)
		array[a]= link;

	qsort(array, size, sizeof(void*), cmp);

	listbase->first= listbase->last= NULL;
	for(a=0; a<size; a++) {
		link= array[a];
		link->next= link->prev= NULL;
		rna_addtail(listbase, link);
	}

	MEM_freeN(array);
}

/* Preprocessing */

static void rna_print_c_string(FILE *f, const char *str)
{
	static const char *escape[] = {"\''", "\"\"", "\??", "\\\\","\aa", "\bb", "\ff", "\nn", "\rr", "\tt", "\vv", NULL};
	int i, j;

	if(!str) {
		fprintf(f, "NULL");
		return;
	}

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

static void rna_print_data_get(FILE *f, PropertyDefRNA *dp)
{
	if(dp->dnastructfromname && dp->dnastructfromprop)
		fprintf(f, "	%s *data= (%s*)(((%s*)ptr->data)->%s);\n", dp->dnastructname, dp->dnastructname, dp->dnastructfromname, dp->dnastructfromprop);
	else
		fprintf(f, "	%s *data= (%s*)(ptr->data);\n", dp->dnastructname, dp->dnastructname);
}

static void rna_print_id_get(FILE *f, PropertyDefRNA *dp)
{
	fprintf(f, "	ID *id= ptr->id.data;\n");
}

static char *rna_alloc_function_name(const char *structname, const char *propname, const char *type)
{
	AllocDefRNA *alloc;
	char buffer[2048];
	char *result;

	snprintf(buffer, sizeof(buffer), "%s_%s_%s", structname, propname, type);
	result= MEM_callocN(sizeof(char)*strlen(buffer)+1, "rna_alloc_function_name");
	strcpy(result, buffer);

	alloc= MEM_callocN(sizeof(AllocDefRNA), "AllocDefRNA");
	alloc->mem= result;
	rna_addtail(&DefRNA.allocs, alloc);

	return result;
}

static StructRNA *rna_find_struct(const char *identifier)
{
	StructDefRNA *ds;

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next)
		if(strcmp(ds->srna->identifier, identifier)==0)
			return ds->srna;

	return NULL;
}

static const char *rna_find_type(const char *type)
{
	StructDefRNA *ds;

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next)
		if(ds->dnaname && strcmp(ds->dnaname, type)==0)
			return ds->srna->identifier;

	return NULL;
}

static const char *rna_find_dna_type(const char *type)
{
	StructDefRNA *ds;

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next)
		if(strcmp(ds->srna->identifier, type)==0)
			return ds->dnaname;

	return NULL;
}

static const char *rna_type_type_name(PropertyRNA *prop)
{
	switch(prop->type) {
		case PROP_BOOLEAN:
		case PROP_INT:
		case PROP_ENUM:
			return "int";
		case PROP_FLOAT:
			return "float";
		case PROP_STRING:
			if(prop->flag & PROP_THICK_WRAP) {
				return "char*";
			}
			else {
				return "const char*";
			}
		default:
			return NULL;
	}
}

static const char *rna_type_type(PropertyRNA *prop)
{
	const char *type;

	type= rna_type_type_name(prop);

	if(type)
		return type;

	return "PointerRNA";
}

static const char *rna_type_struct(PropertyRNA *prop)
{
	const char *type;

	type= rna_type_type_name(prop);

	if(type)
		return "";

	return "struct ";
}

static const char *rna_parameter_type_name(PropertyRNA *parm)
{
	const char *type;

	type= rna_type_type_name(parm);

	if(type)
		return type;

	switch(parm->type) {
		case PROP_POINTER:  {
			PointerPropertyRNA *pparm= (PointerPropertyRNA*)parm;

			if(parm->flag & PROP_RNAPTR)
				return "PointerRNA";
			else
				return rna_find_dna_type((const char *)pparm->type);
		}
		case PROP_COLLECTION: {
			return "ListBase";
		}
		default:
			return "<error, no type specified>";
	}
}

static int rna_enum_bitmask(PropertyRNA *prop)
{
	EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;
	int a, mask= 0;

	if(eprop->item) {
		for(a=0; a<eprop->totitem; a++)
			if(eprop->item[a].identifier[0])
				mask |= eprop->item[a].value;
	}
	
	return mask;
}

static int rna_color_quantize(PropertyRNA *prop, PropertyDefRNA *dp)
{
	return ( (prop->type == PROP_FLOAT) &&
	         (prop->subtype==PROP_COLOR || prop->subtype==PROP_COLOR_GAMMA) &&
	         (IS_DNATYPE_FLOAT_COMPAT(dp->dnatype) == 0) );
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

static void rna_int_print(FILE *f, int num)
{
	if(num == INT_MIN) fprintf(f, "INT_MIN");
	else if(num == INT_MAX) fprintf(f, "INT_MAX");
	else fprintf(f, "%d", num);
}

static char *rna_def_property_get_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
	char *func;

	if(prop->flag & PROP_IDPROPERTY && manualfunc==NULL)
		return NULL;

	if(!manualfunc) {
		if(!dp->dnastructname || !dp->dnaname) {
			fprintf(stderr, "%s (0): %s.%s has no valid dna info.\n",
			        __func__, srna->identifier, prop->identifier);
			DefRNA.error= 1;
			return NULL;
		}

		/* typecheck,  */
		if(dp->dnatype && *dp->dnatype) {

			if(prop->type == PROP_FLOAT) {
				if(IS_DNATYPE_FLOAT_COMPAT(dp->dnatype) == 0) {
					if(prop->subtype != PROP_COLOR_GAMMA) { /* colors are an exception. these get translated */
						fprintf(stderr, "%s (1): %s.%s is a '%s' but wrapped as type '%s'.\n",
						        __func__, srna->identifier, prop->identifier, dp->dnatype, RNA_property_typename(prop->type));
						DefRNA.error= 1;
						return NULL;
					}
				}
			}
			else if(prop->type == PROP_INT || prop->type == PROP_BOOLEAN || prop->type == PROP_ENUM) {
				if(IS_DNATYPE_INT_COMPAT(dp->dnatype) == 0) {
					fprintf(stderr, "%s (2): %s.%s is a '%s' but wrapped as type '%s'.\n",
					        __func__, srna->identifier, prop->identifier, dp->dnatype, RNA_property_typename(prop->type));
					DefRNA.error= 1;
					return NULL;
				}
			}
		}

	}

	func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

	switch(prop->type) {
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;
			fprintf(f, "void %s(PointerRNA *ptr, char *value)\n", func);
			fprintf(f, "{\n");
			if(manualfunc) {
				fprintf(f, "	%s(ptr, value);\n", manualfunc);
			}
			else {
				const PropertySubType subtype= prop->subtype;
				const char *string_copy_func= (subtype==PROP_FILEPATH ||
				                               subtype==PROP_DIRPATH  ||
				                               subtype==PROP_FILENAME ||
				                               subtype==PROP_BYTESTRING) ?
				            "BLI_strncpy" : "BLI_strncpy_utf8";

				rna_print_data_get(f, dp);
				if(sprop->maxlength)
					fprintf(f, "	%s(value, data->%s, %d);\n", string_copy_func, dp->dnaname, sprop->maxlength);
				else
					fprintf(f, "	%s(value, data->%s, sizeof(data->%s));\n", string_copy_func, dp->dnaname, dp->dnaname);
			}
			fprintf(f, "}\n\n");
			break;
		}
		case PROP_POINTER: {
			fprintf(f, "PointerRNA %s(PointerRNA *ptr)\n", func);
			fprintf(f, "{\n");
			if(manualfunc) {
				fprintf(f, "	return %s(ptr);\n", manualfunc);
			}
			else {
				PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;
				rna_print_data_get(f, dp);
				if(dp->dnapointerlevel == 0)
					fprintf(f, "	return rna_pointer_inherit_refine(ptr, &RNA_%s, &data->%s);\n", (const char*)pprop->type, dp->dnaname);
				else
					fprintf(f, "	return rna_pointer_inherit_refine(ptr, &RNA_%s, data->%s);\n", (const char*)pprop->type, dp->dnaname);
			}
			fprintf(f, "}\n\n");
			break;
		}
		case PROP_COLLECTION: {
			CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;

			fprintf(f, "static PointerRNA %s(CollectionPropertyIterator *iter)\n", func);
			fprintf(f, "{\n");
			if(manualfunc) {
				if(strcmp(manualfunc, "rna_iterator_listbase_get") == 0 ||
				   strcmp(manualfunc, "rna_iterator_array_get") == 0 ||
				   strcmp(manualfunc, "rna_iterator_array_dereference_get") == 0)
					fprintf(f, "	return rna_pointer_inherit_refine(&iter->parent, &RNA_%s, %s(iter));\n", (cprop->item_type)? (const char*)cprop->item_type: "UnknownType", manualfunc);
				else
					fprintf(f, "	return %s(iter);\n", manualfunc);
			}
			fprintf(f, "}\n\n");
			break;
		}
		default:
			if(prop->arraydimension) {
				if(prop->flag & PROP_DYNAMIC)
					fprintf(f, "void %s(PointerRNA *ptr, %s values[])\n", func, rna_type_type(prop));
				else
					fprintf(f, "void %s(PointerRNA *ptr, %s values[%u])\n", func, rna_type_type(prop), prop->totarraylength);
				fprintf(f, "{\n");

				if(manualfunc) {
					fprintf(f, "	%s(ptr, values);\n", manualfunc);
				}
				else {
					rna_print_data_get(f, dp);

					if(prop->flag & PROP_DYNAMIC) {
						char *lenfunc= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get_length");
						fprintf(f, "	int i, arraylen[RNA_MAX_ARRAY_DIMENSION];\n");
						fprintf(f, "	int len= %s(ptr, arraylen);\n\n", lenfunc);
						fprintf(f, "	for(i=0; i<len; i++) {\n");
						MEM_freeN(lenfunc);
					}
					else {
						fprintf(f, "	int i;\n\n");
						fprintf(f, "	for(i=0; i<%u; i++) {\n", prop->totarraylength);
					}

					if(dp->dnaarraylength == 1) {
						if(prop->type == PROP_BOOLEAN && dp->booleanbit)
							fprintf(f, "		values[i]= %s((data->%s & (%d<<i)) != 0);\n", (dp->booleannegative)? "!": "", dp->dnaname, dp->booleanbit);
						else
							fprintf(f, "		values[i]= (%s)%s((&data->%s)[i]);\n", rna_type_type(prop), (dp->booleannegative)? "!": "", dp->dnaname);
					}
					else {
						if(prop->type == PROP_BOOLEAN && dp->booleanbit) {
							fprintf(f, "		values[i]= %s((data->%s[i] & ", (dp->booleannegative)? "!": "", dp->dnaname);
							rna_int_print(f, dp->booleanbit);
							fprintf(f, ") != 0);\n");
						}
						else if(rna_color_quantize(prop, dp))
							fprintf(f, "		values[i]= (%s)(data->%s[i]*(1.0f/255.0f));\n", rna_type_type(prop), dp->dnaname);
						else if(dp->dnatype)
							fprintf(f, "		values[i]= (%s)%s(((%s*)data->%s)[i]);\n", rna_type_type(prop), (dp->booleannegative)? "!": "", dp->dnatype, dp->dnaname);
						else
							fprintf(f, "		values[i]= (%s)%s((data->%s)[i]);\n", rna_type_type(prop), (dp->booleannegative)? "!": "", dp->dnaname);
					}
					fprintf(f, "	}\n");
				}
				fprintf(f, "}\n\n");
			}
			else {
				fprintf(f, "%s %s(PointerRNA *ptr)\n", rna_type_type(prop), func);
				fprintf(f, "{\n");

				if(manualfunc) {
					fprintf(f, "	return %s(ptr);\n", manualfunc);
				}
				else {
					rna_print_data_get(f, dp);
					if(prop->type == PROP_BOOLEAN && dp->booleanbit) {
						fprintf(f, "	return %s(((data->%s) & ", (dp->booleannegative)? "!": "", dp->dnaname);
						rna_int_print(f, dp->booleanbit);
						fprintf(f, ") != 0);\n");
					}
					else if(prop->type == PROP_ENUM && dp->enumbitflags) {
						fprintf(f, "	return ((data->%s) & ", dp->dnaname);
						rna_int_print(f, rna_enum_bitmask(prop));
						fprintf(f, ");\n");
					}
					else
						fprintf(f, "	return (%s)%s(data->%s);\n", rna_type_type(prop), (dp->booleannegative)? "!": "", dp->dnaname);
				}

				fprintf(f, "}\n\n");
			}
			break;
	}

	return func;
}

/* defined min/max variables to be used by rna_clamp_value() */
static void rna_clamp_value_range(FILE *f, PropertyRNA *prop)
{
	if(prop->type == PROP_FLOAT) {
		FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
		if(fprop->range) {
			fprintf(f, "	float prop_clamp_min, prop_clamp_max;\n");
			fprintf(f, "	%s(ptr, &prop_clamp_min, &prop_clamp_max);\n", rna_function_string(fprop->range));
		}
	}
	else if(prop->type == PROP_INT) {
		IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
		if(iprop->range) {
			fprintf(f, "	int prop_clamp_min, prop_clamp_max;\n");
			fprintf(f, "	%s(ptr, &prop_clamp_min, &prop_clamp_max);\n", rna_function_string(iprop->range));
		}
	}
}

static void rna_clamp_value(FILE *f, PropertyRNA *prop, int array)
{
	if(prop->type == PROP_INT) {
		IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

		if(iprop->hardmin != INT_MIN || iprop->hardmax != INT_MAX) {
			if(array) fprintf(f, "CLAMPIS(values[i], ");
			else fprintf(f, "CLAMPIS(value, ");
			if(iprop->range) {
				fprintf(f, "prop_clamp_min, prop_clamp_max);");
			}
			else {
				rna_int_print(f, iprop->hardmin); fprintf(f, ", ");
				rna_int_print(f, iprop->hardmax); fprintf(f, ");\n");
			}
			return;
		}
	}
	else if(prop->type == PROP_FLOAT) {
		FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

		if(fprop->hardmin != -FLT_MAX || fprop->hardmax != FLT_MAX) {
			if(array) fprintf(f, "CLAMPIS(values[i], ");
			else fprintf(f, "CLAMPIS(value, ");
			if(fprop->range) {
				fprintf(f, "prop_clamp_min, prop_clamp_max);");
			}
			else {
				rna_float_print(f, fprop->hardmin); fprintf(f, ", ");
				rna_float_print(f, fprop->hardmax); fprintf(f, ");\n");
			}
			return;
		}
	}

	if(array)
		fprintf(f, "values[i];\n");
	else
		fprintf(f, "value;\n");
}

static char *rna_def_property_set_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
	char *func;

	if(!(prop->flag & PROP_EDITABLE))
		return NULL;
	if(prop->flag & PROP_IDPROPERTY && manualfunc==NULL)
		return NULL;

	if(!manualfunc) {
		if(!dp->dnastructname || !dp->dnaname) {
			if(prop->flag & PROP_EDITABLE) {
				fprintf(stderr, "%s: %s.%s has no valid dna info.\n",
				        __func__, srna->identifier, prop->identifier);
				DefRNA.error= 1;
			}
			return NULL;
		}
	}

	func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "set");

	switch(prop->type) {
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;
			fprintf(f, "void %s(PointerRNA *ptr, const char *value)\n", func);
			fprintf(f, "{\n");
			if(manualfunc) {
				fprintf(f, "	%s(ptr, value);\n", manualfunc);
			}
			else {
				const PropertySubType subtype= prop->subtype;
				const char *string_copy_func= (subtype==PROP_FILEPATH ||
				                               subtype==PROP_DIRPATH  ||
				                               subtype==PROP_FILENAME ||
				                               subtype==PROP_BYTESTRING) ?
				            "BLI_strncpy" : "BLI_strncpy_utf8";

				rna_print_data_get(f, dp);
				if(sprop->maxlength)
					fprintf(f, "	%s(data->%s, value, %d);\n", string_copy_func, dp->dnaname, sprop->maxlength);
				else
					fprintf(f, "	%s(data->%s, value, sizeof(data->%s));\n", string_copy_func, dp->dnaname, dp->dnaname);
			}
			fprintf(f, "}\n\n");
			break;
		}
		case PROP_POINTER: {
			fprintf(f, "void %s(PointerRNA *ptr, PointerRNA value)\n", func);
			fprintf(f, "{\n");
			if(manualfunc) {
				fprintf(f, "	%s(ptr, value);\n", manualfunc);
			}
			else {
				rna_print_data_get(f, dp);

				if(prop->flag & PROP_ID_SELF_CHECK) {
					rna_print_id_get(f, dp);
					fprintf(f, "	if(id==value.data) return;\n\n");
				}

				if(prop->flag & PROP_ID_REFCOUNT) {
					fprintf(f, "\n	if(data->%s)\n", dp->dnaname);
					fprintf(f, "		id_us_min((ID*)data->%s);\n", dp->dnaname);
					fprintf(f, "	if(value.data)\n");
					fprintf(f, "		id_us_plus((ID*)value.data);\n\n");
				}
				else {
					PointerPropertyRNA *pprop= (PointerPropertyRNA*)dp->prop;
					StructRNA *type= rna_find_struct((const char*)pprop->type);
					if(type && (type->flag & STRUCT_ID)) {
						fprintf(f, "	if(value.data)\n");
						fprintf(f, "		id_lib_extern((ID*)value.data);\n\n");
					}
				}

				fprintf(f, "	data->%s= value.data;\n", dp->dnaname);

			}
			fprintf(f, "}\n\n");
			break;
		}
		default:
			if(prop->arraydimension) {
				if(prop->flag & PROP_DYNAMIC)
					fprintf(f, "void %s(PointerRNA *ptr, const %s values[])\n", func, rna_type_type(prop));
				else
					fprintf(f, "void %s(PointerRNA *ptr, const %s values[%u])\n", func, rna_type_type(prop), prop->totarraylength);
				fprintf(f, "{\n");

				if(manualfunc) {
					fprintf(f, "	%s(ptr, values);\n", manualfunc);
				}
				else {
					rna_print_data_get(f, dp);

					if(prop->flag & PROP_DYNAMIC) {
						char *lenfunc= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "set_length");
						fprintf(f, "	int i, arraylen[RNA_MAX_ARRAY_DIMENSION];\n");
						fprintf(f, "	int len= %s(ptr, arraylen);\n\n", lenfunc);
						rna_clamp_value_range(f, prop);
						fprintf(f, "	for(i=0; i<len; i++) {\n");
						MEM_freeN(lenfunc);
					}
					else {
						fprintf(f, "	int i;\n\n");
						rna_clamp_value_range(f, prop);
						fprintf(f, "	for(i=0; i<%u; i++) {\n", prop->totarraylength);
					}

					if(dp->dnaarraylength == 1) {
						if(prop->type == PROP_BOOLEAN && dp->booleanbit) {
							fprintf(f, "		if(%svalues[i]) data->%s |= (%d<<i);\n", (dp->booleannegative)? "!": "", dp->dnaname, dp->booleanbit);
							fprintf(f, "		else data->%s &= ~(%d<<i);\n", dp->dnaname, dp->booleanbit);
						}
						else {
							fprintf(f, "		(&data->%s)[i]= %s", dp->dnaname, (dp->booleannegative)? "!": "");
							rna_clamp_value(f, prop, 1);
						}
					}
					else {
						if(prop->type == PROP_BOOLEAN && dp->booleanbit) {
							fprintf(f, "		if(%svalues[i]) data->%s[i] |= ", (dp->booleannegative)? "!": "", dp->dnaname);
							rna_int_print(f, dp->booleanbit);
							fprintf(f, ";\n");
							fprintf(f, "		else data->%s[i] &= ~", dp->dnaname);
							rna_int_print(f, dp->booleanbit);
							fprintf(f, ";\n");
						}
						else if(rna_color_quantize(prop, dp)) {
							fprintf(f, "		data->%s[i]= FTOCHAR(values[i]);\n", dp->dnaname);
						}
						else {
							if(dp->dnatype)
								fprintf(f, "		((%s*)data->%s)[i]= %s", dp->dnatype, dp->dnaname, (dp->booleannegative)? "!": "");
							else
								fprintf(f, "		(data->%s)[i]= %s", dp->dnaname, (dp->booleannegative)? "!": "");
							rna_clamp_value(f, prop, 1);
						}
					}
					fprintf(f, "	}\n");
				}
				fprintf(f, "}\n\n");
			}
			else {
				fprintf(f, "void %s(PointerRNA *ptr, %s value)\n", func, rna_type_type(prop));
				fprintf(f, "{\n");

				if(manualfunc) {
					fprintf(f, "	%s(ptr, value);\n", manualfunc);
				}
				else {
					rna_print_data_get(f, dp);
					if(prop->type == PROP_BOOLEAN && dp->booleanbit) {
						fprintf(f, "	if(%svalue) data->%s |= ", (dp->booleannegative)? "!": "", dp->dnaname);
						rna_int_print(f, dp->booleanbit);
						fprintf(f, ";\n");
						fprintf(f, "	else data->%s &= ~", dp->dnaname);
						rna_int_print(f, dp->booleanbit);
						fprintf(f, ";\n");
					}
					else if(prop->type == PROP_ENUM && dp->enumbitflags) {
						fprintf(f, "	data->%s &= ~", dp->dnaname);
						rna_int_print(f, rna_enum_bitmask(prop));
						fprintf(f, ";\n");
						fprintf(f, "	data->%s |= value;\n", dp->dnaname);
					}
					else {
						rna_clamp_value_range(f, prop);
						fprintf(f, "	data->%s= %s", dp->dnaname, (dp->booleannegative)? "!": "");
						rna_clamp_value(f, prop, 0);
					}
				}
				fprintf(f, "}\n\n");
			}
			break;
	}

	return func;
}

static char *rna_def_property_length_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
	char *func= NULL;

	if(prop->flag & PROP_IDPROPERTY && manualfunc==NULL)
		return NULL;

	if(prop->type == PROP_STRING) {
		if(!manualfunc) {
			if(!dp->dnastructname || !dp->dnaname) {
				fprintf(stderr, "%s: %s.%s has no valid dna info.\n",
				        __func__, srna->identifier, prop->identifier);
				DefRNA.error= 1;
				return NULL;
			}
		}

		func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "length");

		fprintf(f, "int %s(PointerRNA *ptr)\n", func);
		fprintf(f, "{\n");
		if(manualfunc) {
			fprintf(f, "	return %s(ptr);\n", manualfunc);
		}
		else {
			rna_print_data_get(f, dp);
			fprintf(f, "	return strlen(data->%s);\n", dp->dnaname);
		}
		fprintf(f, "}\n\n");
	}
	else if(prop->type == PROP_COLLECTION) {
		if(!manualfunc) {
			if(prop->type == PROP_COLLECTION && (!(dp->dnalengthname || dp->dnalengthfixed)|| !dp->dnaname)) {
				fprintf(stderr, "%s: %s.%s has no valid dna info.\n",
				        __func__, srna->identifier, prop->identifier);
				DefRNA.error= 1;
				return NULL;
			}
		}

		func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "length");

		fprintf(f, "int %s(PointerRNA *ptr)\n", func);
		fprintf(f, "{\n");
		if(manualfunc) {
			fprintf(f, "	return %s(ptr);\n", manualfunc);
		}
		else {
			rna_print_data_get(f, dp);
			if(dp->dnalengthname)
				fprintf(f, "	return (data->%s == NULL)? 0: data->%s;\n", dp->dnaname, dp->dnalengthname);
			else
				fprintf(f, "	return (data->%s == NULL)? 0: %d;\n", dp->dnaname, dp->dnalengthfixed);
		}
		fprintf(f, "}\n\n");
	}

	return func;
}

static char *rna_def_property_begin_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
	char *func, *getfunc;

	if(prop->flag & PROP_IDPROPERTY && manualfunc==NULL)
		return NULL;

	if(!manualfunc) {
		if(!dp->dnastructname || !dp->dnaname) {
			fprintf(stderr, "%s: %s.%s has no valid dna info.\n",
			        __func__, srna->identifier, prop->identifier);
			DefRNA.error= 1;
			return NULL;
		}
	}

	func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "begin");

	fprintf(f, "void %s(CollectionPropertyIterator *iter, PointerRNA *ptr)\n", func);
	fprintf(f, "{\n");

	if(!manualfunc)
		rna_print_data_get(f, dp);

	fprintf(f, "\n	memset(iter, 0, sizeof(*iter));\n");
	fprintf(f, "	iter->parent= *ptr;\n");
	fprintf(f, "	iter->prop= (PropertyRNA*)&rna_%s_%s;\n", srna->identifier, prop->identifier);

	if(dp->dnalengthname || dp->dnalengthfixed) {
		if(manualfunc) {
			fprintf(f, "\n	%s(iter, ptr);\n", manualfunc);
		}
		else {
			if(dp->dnalengthname)
				fprintf(f, "\n	rna_iterator_array_begin(iter, data->%s, sizeof(data->%s[0]), data->%s, 0, NULL);\n", dp->dnaname, dp->dnaname, dp->dnalengthname);
			else
				fprintf(f, "\n	rna_iterator_array_begin(iter, data->%s, sizeof(data->%s[0]), %d, 0, NULL);\n", dp->dnaname, dp->dnaname, dp->dnalengthfixed);
		}
	}
	else {
		if(manualfunc)
			fprintf(f, "\n	%s(iter, ptr);\n", manualfunc);
		else if(dp->dnapointerlevel == 0)
			fprintf(f, "\n	rna_iterator_listbase_begin(iter, &data->%s, NULL);\n", dp->dnaname);
		else
			fprintf(f, "\n	rna_iterator_listbase_begin(iter, data->%s, NULL);\n", dp->dnaname);
	}

	getfunc= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

	fprintf(f, "\n	if(iter->valid)\n");
	fprintf(f, "		iter->ptr= %s(iter);\n", getfunc);

	fprintf(f, "}\n\n");


	return func;
}

static char *rna_def_property_lookup_int_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc, const char *nextfunc)
{
	/* note on indices, this is for external functions and ignores skipped values.
	 * so the the index can only be checked against the length when there is no 'skip' funcion. */
	char *func;

	if(prop->flag & PROP_IDPROPERTY && manualfunc==NULL)
		return NULL;

	if(!manualfunc) {
		if(!dp->dnastructname || !dp->dnaname)
			return NULL;

		/* only supported in case of standard next functions */
		if(strcmp(nextfunc, "rna_iterator_array_next") == 0);
		else if(strcmp(nextfunc, "rna_iterator_listbase_next") == 0);
		else return NULL;
	}

	func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "lookup_int");

	fprintf(f, "int %s(PointerRNA *ptr, int index, PointerRNA *r_ptr)\n", func);
	fprintf(f, "{\n");

	if(manualfunc) {
		fprintf(f, "\n	return %s(ptr, index, r_ptr);\n", manualfunc);
		fprintf(f, "}\n\n");
		return func;
	}

	fprintf(f, "	int found= 0;\n");
	fprintf(f, "	CollectionPropertyIterator iter;\n\n");

	fprintf(f, "	%s_%s_begin(&iter, ptr);\n\n", srna->identifier, rna_safe_id(prop->identifier));
	fprintf(f, "	if(iter.valid){\n");

	if(strcmp(nextfunc, "rna_iterator_array_next") == 0) {
		fprintf(f, "		ArrayIterator *internal= iter.internal;\n");
		fprintf(f, "		if(index < 0 || index >= internal->length) {\n");
		fprintf(f, "#ifdef __GNUC__\n");
		fprintf(f, "			printf(\"Array iterator out of range: %%s (index %%d)\\n\", __func__, index);\n");
		fprintf(f, "#else\n");
		fprintf(f, "			printf(\"Array iterator out of range: (index %%d)\\n\", index);\n");
		fprintf(f, "#endif\n");
		fprintf(f, "		}\n");
		fprintf(f, "		else if(internal->skip) {\n");
		fprintf(f, "			while(index-- > 0 && iter.valid) {\n");
		fprintf(f, "				rna_iterator_array_next(&iter);\n");
		fprintf(f, "			}\n");
		fprintf(f, "			found= (index == -1 && iter.valid);\n");
		fprintf(f, "		}\n");
		fprintf(f, "		else {\n");
		fprintf(f, "			internal->ptr += internal->itemsize*index;\n");
		fprintf(f, "			found= 1;\n");
		fprintf(f, "		}\n");
	}
	else if(strcmp(nextfunc, "rna_iterator_listbase_next") == 0) {
		fprintf(f, "		ListBaseIterator *internal= iter.internal;\n");
		fprintf(f, "		if(internal->skip) {\n");
		fprintf(f, "			while(index-- > 0 && iter.valid) {\n");
		fprintf(f, "				rna_iterator_listbase_next(&iter);\n");
		fprintf(f, "			}\n");
		fprintf(f, "			found= (index == -1 && iter.valid);\n");
		fprintf(f, "		}\n");
		fprintf(f, "		else {\n");
		fprintf(f, "			while(index-- > 0 && internal->link)\n");
		fprintf(f, "				internal->link= internal->link->next;\n");
		fprintf(f, "			found= (index == -1 && internal->link);\n");
		fprintf(f, "		}\n");
	}

	fprintf(f, "		if(found) *r_ptr = %s_%s_get(&iter);\n", srna->identifier, rna_safe_id(prop->identifier));
	fprintf(f, "	}\n\n");
	fprintf(f, "	%s_%s_end(&iter);\n\n", srna->identifier, rna_safe_id(prop->identifier));

	fprintf(f, "	return found;\n");

#if 0
	rna_print_data_get(f, dp);
	item_type= (cprop->item_type)? (const char*)cprop->item_type: "UnknownType";

	if(dp->dnalengthname || dp->dnalengthfixed) {
		if(dp->dnalengthname)
			fprintf(f, "\n	rna_array_lookup_int(ptr, &RNA_%s, data->%s, sizeof(data->%s[0]), data->%s, index);\n", item_type, dp->dnaname, dp->dnaname, dp->dnalengthname);
		else
			fprintf(f, "\n	rna_array_lookup_int(ptr, &RNA_%s, data->%s, sizeof(data->%s[0]), %d, index);\n", item_type, dp->dnaname, dp->dnaname, dp->dnalengthfixed);
	}
	else {
		if(dp->dnapointerlevel == 0)
			fprintf(f, "\n	return rna_listbase_lookup_int(ptr, &RNA_%s, &data->%s, index);\n", item_type, dp->dnaname);
		else
			fprintf(f, "\n	return rna_listbase_lookup_int(ptr, &RNA_%s, data->%s, index);\n", item_type, dp->dnaname);
	}
#endif

	fprintf(f, "}\n\n");

	return func;
}

static char *rna_def_property_next_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
	char *func, *getfunc;

	if(prop->flag & PROP_IDPROPERTY && manualfunc==NULL)
		return NULL;

	if(!manualfunc)
		return NULL;

	func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "next");

	fprintf(f, "void %s(CollectionPropertyIterator *iter)\n", func);
	fprintf(f, "{\n");
	fprintf(f, "	%s(iter);\n", manualfunc);

	getfunc= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

	fprintf(f, "\n	if(iter->valid)\n");
	fprintf(f, "		iter->ptr= %s(iter);\n", getfunc);

	fprintf(f, "}\n\n");

	return func;
}

static char *rna_def_property_end_func(FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
	char *func;

	if(prop->flag & PROP_IDPROPERTY && manualfunc==NULL)
		return NULL;

	func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "end");

	fprintf(f, "void %s(CollectionPropertyIterator *iter)\n", func);
	fprintf(f, "{\n");
	if(manualfunc)
		fprintf(f, "	%s(iter);\n", manualfunc);
	fprintf(f, "}\n\n");

	return func;
}

static void rna_set_raw_property(PropertyDefRNA *dp, PropertyRNA *prop)
{
	if(dp->dnapointerlevel != 0)
		return;
	if(!dp->dnatype || !dp->dnaname || !dp->dnastructname)
		return;
	
	if(strcmp(dp->dnatype, "char") == 0) {
		prop->rawtype= PROP_RAW_CHAR;
		prop->flag |= PROP_RAW_ACCESS;
	}
	else if(strcmp(dp->dnatype, "short") == 0) {
		prop->rawtype= PROP_RAW_SHORT;
		prop->flag |= PROP_RAW_ACCESS;
	}
	else if(strcmp(dp->dnatype, "int") == 0) {
		prop->rawtype= PROP_RAW_INT;
		prop->flag |= PROP_RAW_ACCESS;
	}
	else if(strcmp(dp->dnatype, "float") == 0) {
		prop->rawtype= PROP_RAW_FLOAT;
		prop->flag |= PROP_RAW_ACCESS;
	}
	else if(strcmp(dp->dnatype, "double") == 0) {
		prop->rawtype= PROP_RAW_DOUBLE;
		prop->flag |= PROP_RAW_ACCESS;
	}
}

static void rna_set_raw_offset(FILE *f, StructRNA *srna, PropertyRNA *prop)
{
	PropertyDefRNA *dp= rna_find_struct_property_def(srna, prop);

	fprintf(f, "\toffsetof(%s, %s), %d", dp->dnastructname, dp->dnaname, prop->rawtype);
}

static void rna_def_property_funcs(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
	PropertyRNA *prop;

	prop= dp->prop;

	switch(prop->type) {
		case PROP_BOOLEAN: {
			BoolPropertyRNA *bprop= (BoolPropertyRNA*)prop;

			if(!prop->arraydimension) {
				if(!bprop->get && !bprop->set && !dp->booleanbit)
					rna_set_raw_property(dp, prop);

				bprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)bprop->get);
				bprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)bprop->set);
			}
			else {
				bprop->getarray= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)bprop->getarray);
				bprop->setarray= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)bprop->setarray);
			}
			break;
		}
		case PROP_INT: {
			IntPropertyRNA *iprop= (IntPropertyRNA*)prop;

			if(!prop->arraydimension) {
				if(!iprop->get && !iprop->set)
					rna_set_raw_property(dp, prop);

				iprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)iprop->get);
				iprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)iprop->set);
			}
			else {
				if(!iprop->getarray && !iprop->setarray)
					rna_set_raw_property(dp, prop);

				iprop->getarray= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)iprop->getarray);
				iprop->setarray= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)iprop->setarray);
			}
			break;
		}
		case PROP_FLOAT: {
			FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;

			if(!prop->arraydimension) {
				if(!fprop->get && !fprop->set)
					rna_set_raw_property(dp, prop);

				fprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)fprop->get);
				fprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)fprop->set);
			}
			else {
				if(!fprop->getarray && !fprop->setarray)
					rna_set_raw_property(dp, prop);

				fprop->getarray= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)fprop->getarray);
				fprop->setarray= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)fprop->setarray);
			}
			break;
		}
		case PROP_ENUM: {
			EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;

			eprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)eprop->get);
			eprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)eprop->set);
			break;
		}
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

			sprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)sprop->get);
			sprop->length= (void*)rna_def_property_length_func(f, srna, prop, dp, (const char*)sprop->length);
			sprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)sprop->set);
			break;
		}
		case PROP_POINTER: {
			PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;

			pprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)pprop->get);
			pprop->set= (void*)rna_def_property_set_func(f, srna, prop, dp, (const char*)pprop->set);
			if(!pprop->type) {
				fprintf(stderr, "%s: %s.%s, pointer must have a struct type.\n",
				        __func__, srna->identifier, prop->identifier);
				DefRNA.error= 1;
			}
			break;
		}
		case PROP_COLLECTION: {
			CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;
			const char *nextfunc= (const char*)cprop->next;

			if(dp->dnatype && strcmp(dp->dnatype, "ListBase")==0);
			else if(dp->dnalengthname || dp->dnalengthfixed)
				cprop->length= (void*)rna_def_property_length_func(f, srna, prop, dp, (const char*)cprop->length);

			/* test if we can allow raw array access, if it is using our standard
			 * array get/next function, we can be sure it is an actual array */
			if(cprop->next && cprop->get)
				if(strcmp((const char*)cprop->next, "rna_iterator_array_next") == 0 &&
				   strcmp((const char*)cprop->get, "rna_iterator_array_get") == 0)
					prop->flag |= PROP_RAW_ARRAY;

			cprop->get= (void*)rna_def_property_get_func(f, srna, prop, dp, (const char*)cprop->get);
			cprop->begin= (void*)rna_def_property_begin_func(f, srna, prop, dp, (const char*)cprop->begin);
			cprop->next= (void*)rna_def_property_next_func(f, srna, prop, dp, (const char*)cprop->next);
			cprop->end= (void*)rna_def_property_end_func(f, srna, prop, dp, (const char*)cprop->end);
			cprop->lookupint= (void*)rna_def_property_lookup_int_func(f, srna, prop, dp, (const char*)cprop->lookupint, nextfunc);

			if(!(prop->flag & PROP_IDPROPERTY)) {
				if(!cprop->begin) {
					fprintf(stderr, "%s: %s.%s, collection must have a begin function.\n",
					        __func__, srna->identifier, prop->identifier);
					DefRNA.error= 1;
				}
				if(!cprop->next) {
					fprintf(stderr, "%s: %s.%s, collection must have a next function.\n",
					        __func__, srna->identifier, prop->identifier);
					DefRNA.error= 1;
				}
				if(!cprop->get) {
					fprintf(stderr, "%s: %s.%s, collection must have a get function.\n",
					        __func__, srna->identifier, prop->identifier);
					DefRNA.error= 1;
				}
			}
			if(!cprop->item_type) {
				fprintf(stderr, "%s: %s.%s, collection must have a struct type.\n",
				        __func__, srna->identifier, prop->identifier);
				DefRNA.error= 1;
			}
			break;
		}
	}
}

static void rna_def_property_funcs_header(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
	PropertyRNA *prop;
	char *func;

	prop= dp->prop;

	if(prop->flag & (PROP_IDPROPERTY|PROP_BUILTIN))
		return;

	func= rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "");

	switch(prop->type) {
		case PROP_BOOLEAN:
		case PROP_INT: {
			if(!prop->arraydimension) {
				fprintf(f, "int %sget(PointerRNA *ptr);\n", func);
				//fprintf(f, "void %sset(PointerRNA *ptr, int value);\n", func);
			}
			else if(prop->arraydimension && prop->totarraylength) {
				fprintf(f, "void %sget(PointerRNA *ptr, int values[%u]);\n", func, prop->totarraylength);
				//fprintf(f, "void %sset(PointerRNA *ptr, const int values[%d]);\n", func, prop->arraylength);
			}
			else {
				fprintf(f, "void %sget(PointerRNA *ptr, int values[]);\n", func);
				//fprintf(f, "void %sset(PointerRNA *ptr, const int values[]);\n", func);
			}
			break;
		}
		case PROP_FLOAT: {
			if(!prop->arraydimension) {
				fprintf(f, "float %sget(PointerRNA *ptr);\n", func);
				//fprintf(f, "void %sset(PointerRNA *ptr, float value);\n", func);
			}
			else if(prop->arraydimension && prop->totarraylength) {
				fprintf(f, "void %sget(PointerRNA *ptr, float values[%u]);\n", func, prop->totarraylength);
				//fprintf(f, "void %sset(PointerRNA *ptr, const float values[%d]);\n", func, prop->arraylength);
			}
			else {
				fprintf(f, "void %sget(PointerRNA *ptr, float values[]);\n", func);
				//fprintf(f, "void %sset(PointerRNA *ptr, const float values[]);\n", func);
			}
			break;
		}
		case PROP_ENUM: {
			EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;
			int i;

			if(eprop->item) {
				fprintf(f, "enum {\n");

				for(i=0; i<eprop->totitem; i++)
					if(eprop->item[i].identifier[0])
						fprintf(f, "\t%s_%s_%s = %d,\n", srna->identifier, prop->identifier, eprop->item[i].identifier, eprop->item[i].value);

				fprintf(f, "};\n\n");
			}

			fprintf(f, "int %sget(PointerRNA *ptr);\n", func);
			//fprintf(f, "void %sset(PointerRNA *ptr, int value);\n", func);

			break;
		}
		case PROP_STRING: {
			StringPropertyRNA *sprop= (StringPropertyRNA*)prop;

			if(sprop->maxlength) {
				fprintf(f, "#define %s_%s_MAX %d\n\n", srna->identifier, prop->identifier, sprop->maxlength);
			}
			
			fprintf(f, "void %sget(PointerRNA *ptr, char *value);\n", func);
			fprintf(f, "int %slength(PointerRNA *ptr);\n", func);
			//fprintf(f, "void %sset(PointerRNA *ptr, const char *value);\n", func);

			break;
		}
		case PROP_POINTER: {
			fprintf(f, "PointerRNA %sget(PointerRNA *ptr);\n", func);
			//fprintf(f, "void %sset(PointerRNA *ptr, PointerRNA value);\n", func);
			break;
		}
		case PROP_COLLECTION: {
			fprintf(f, "void %sbegin(CollectionPropertyIterator *iter, PointerRNA *ptr);\n", func);
			fprintf(f, "void %snext(CollectionPropertyIterator *iter);\n", func);
			fprintf(f, "void %send(CollectionPropertyIterator *iter);\n", func);
			//fprintf(f, "int %slength(PointerRNA *ptr);\n", func);
			//fprintf(f, "void %slookup_int(PointerRNA *ptr, int key, StructRNA **type);\n", func);
			//fprintf(f, "void %slookup_string(PointerRNA *ptr, const char *key, StructRNA **type);\n", func);
			break;
		}
	}

	fprintf(f, "\n");
}

static void rna_def_property_funcs_header_cpp(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
	PropertyRNA *prop;

	prop= dp->prop;

	if(prop->flag & (PROP_IDPROPERTY|PROP_BUILTIN))
		return;
	
	if(prop->name && prop->description && prop->description[0] != '\0')
		fprintf(f, "\t/* %s: %s */\n", prop->name, prop->description);
	else if(prop->name)
		fprintf(f, "\t/* %s */\n", prop->name);
	else
		fprintf(f, "\t/* */\n");

	switch(prop->type) {
		case PROP_BOOLEAN: {
			if(!prop->arraydimension)
				fprintf(f, "\tinline bool %s(void);", rna_safe_id(prop->identifier));
			else if(prop->totarraylength)
				fprintf(f, "\tinline Array<int, %u> %s(void);", prop->totarraylength, rna_safe_id(prop->identifier));
			break;
		}
		case PROP_INT: {
			if(!prop->arraydimension)
				fprintf(f, "\tinline int %s(void);", rna_safe_id(prop->identifier));
			else if(prop->totarraylength)
				fprintf(f, "\tinline Array<int, %u> %s(void);", prop->totarraylength, rna_safe_id(prop->identifier));
			break;
		}
		case PROP_FLOAT: {
			if(!prop->arraydimension)
				fprintf(f, "\tinline float %s(void);", rna_safe_id(prop->identifier));
			else if(prop->totarraylength)
				fprintf(f, "\tinline Array<float, %u> %s(void);", prop->totarraylength, rna_safe_id(prop->identifier));
			break;
		}
		case PROP_ENUM: {
			EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;
			int i;

			if(eprop->item) {
				fprintf(f, "\tenum %s_enum {\n", rna_safe_id(prop->identifier));

				for(i=0; i<eprop->totitem; i++)
					if(eprop->item[i].identifier[0])
						fprintf(f, "\t\t%s_%s = %d,\n", rna_safe_id(prop->identifier), eprop->item[i].identifier, eprop->item[i].value);

				fprintf(f, "\t};\n");
			}

			fprintf(f, "\tinline %s_enum %s(void);", rna_safe_id(prop->identifier), rna_safe_id(prop->identifier));
			break;
		}
		case PROP_STRING: {
			fprintf(f, "\tinline std::string %s(void);", rna_safe_id(prop->identifier));
			break;
		}
		case PROP_POINTER: {
			PointerPropertyRNA *pprop= (PointerPropertyRNA*)dp->prop;

			if(pprop->type)
				fprintf(f, "\tinline %s %s(void);", (const char*)pprop->type, rna_safe_id(prop->identifier));
			else
				fprintf(f, "\tinline %s %s(void);", "UnknownType", rna_safe_id(prop->identifier));
			break;
		}
		case PROP_COLLECTION: {
			CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)dp->prop;

			if(cprop->item_type)
				fprintf(f, "\tCOLLECTION_PROPERTY(%s, %s, %s)", (const char*)cprop->item_type, srna->identifier, rna_safe_id(prop->identifier));
			else
				fprintf(f, "\tCOLLECTION_PROPERTY(%s, %s, %s)", "UnknownType", srna->identifier, rna_safe_id(prop->identifier));
			break;
		}
	}

	fprintf(f, "\n");
}

static void rna_def_property_funcs_impl_cpp(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
	PropertyRNA *prop;

	prop= dp->prop;

	if(prop->flag & (PROP_IDPROPERTY|PROP_BUILTIN))
		return;

	switch(prop->type) {
		case PROP_BOOLEAN: {
			if(!prop->arraydimension)
				fprintf(f, "\tBOOLEAN_PROPERTY(%s, %s)", srna->identifier, rna_safe_id(prop->identifier));
			else if(prop->totarraylength)
				fprintf(f, "\tBOOLEAN_ARRAY_PROPERTY(%s, %u, %s)", srna->identifier, prop->totarraylength, rna_safe_id(prop->identifier));
			break;
		}
		case PROP_INT: {
			if(!prop->arraydimension)
				fprintf(f, "\tINT_PROPERTY(%s, %s)", srna->identifier, rna_safe_id(prop->identifier));
			else if(prop->totarraylength)
				fprintf(f, "\tINT_ARRAY_PROPERTY(%s, %u, %s)", srna->identifier, prop->totarraylength, rna_safe_id(prop->identifier));
			break;
		}
		case PROP_FLOAT: {
			if(!prop->arraydimension)
				fprintf(f, "\tFLOAT_PROPERTY(%s, %s)", srna->identifier, rna_safe_id(prop->identifier));
			else if(prop->totarraylength)
				fprintf(f, "\tFLOAT_ARRAY_PROPERTY(%s, %u, %s)", srna->identifier, prop->totarraylength, rna_safe_id(prop->identifier));
			break;
		}
		case PROP_ENUM: {
			fprintf(f, "\tENUM_PROPERTY(%s_enum, %s, %s)", rna_safe_id(prop->identifier), srna->identifier, rna_safe_id(prop->identifier));

			break;
		}
		case PROP_STRING: {
			fprintf(f, "\tSTRING_PROPERTY(%s, %s)", srna->identifier, rna_safe_id(prop->identifier));
			break;
		}
		case PROP_POINTER: {
			PointerPropertyRNA *pprop= (PointerPropertyRNA*)dp->prop;

			if(pprop->type)
				fprintf(f, "\tPOINTER_PROPERTY(%s, %s, %s)", (const char*)pprop->type, srna->identifier, rna_safe_id(prop->identifier));
			else
				fprintf(f, "\tPOINTER_PROPERTY(%s, %s, %s)", "UnknownType", srna->identifier, rna_safe_id(prop->identifier));
			break;
		}
		case PROP_COLLECTION: {
			/*CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)dp->prop;

			if(cprop->type)
				fprintf(f, "\tCOLLECTION_PROPERTY(%s, %s, %s)", (const char*)cprop->type, srna->identifier, prop->identifier);
			else
				fprintf(f, "\tCOLLECTION_PROPERTY(%s, %s, %s)", "UnknownType", srna->identifier, prop->identifier);*/
			break;
		}
	}

	fprintf(f, "\n");
}

static void rna_def_function_funcs(FILE *f, StructDefRNA *dsrna, FunctionDefRNA *dfunc)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyDefRNA *dparm;
	PropertyType type;
	const char *funcname, *valstr;
	const char *ptrstr;
	const short has_data= (dfunc->cont.properties.first != NULL);
	int flag, pout, cptr, first;

	srna= dsrna->srna;
	func= dfunc->func;

	if(!dfunc->call)
		return;

	funcname= rna_alloc_function_name(srna->identifier, func->identifier, "call");

	/* function definition */
	fprintf(f, "void %s(bContext *C, ReportList *reports, PointerRNA *_ptr, ParameterList *_parms)", funcname);
	fprintf(f, "\n{\n");

	/* variable definitions */
	
	if(func->flag & FUNC_USE_SELF_ID) {
		fprintf(f, "\tstruct ID *_selfid;\n");
	}

	if((func->flag & FUNC_NO_SELF)==0) {
		if(dsrna->dnaname) fprintf(f, "\tstruct %s *_self;\n", dsrna->dnaname);
		else fprintf(f, "\tstruct %s *_self;\n", srna->identifier);
	}

	dparm= dfunc->cont.properties.first;
	for(; dparm; dparm= dparm->next) {
		type = dparm->prop->type;
		flag = dparm->prop->flag;
		pout = (flag & PROP_OUTPUT);
		cptr = ((type == PROP_POINTER) && !(flag & PROP_RNAPTR));

		if(dparm->prop==func->c_ret)
			ptrstr= cptr || dparm->prop->arraydimension ? "*" : "";
		/* XXX only arrays and strings are allowed to be dynamic, is this checked anywhere? */
		else if (cptr || (flag & PROP_DYNAMIC))
			ptrstr= pout ? "**" : "*";
		/* fixed size arrays and RNA pointers are pre-allocated on the ParameterList stack, pass a pointer to it */
		else if (type == PROP_POINTER || dparm->prop->arraydimension)
			ptrstr= "*";
		/* PROP_THICK_WRAP strings are pre-allocated on the ParameterList stack, but type name for string props is already char*, so leave empty */
		else if (type == PROP_STRING && (flag & PROP_THICK_WRAP))
			ptrstr= "";
		else
			ptrstr= pout ? "*" : "";

		/* for dynamic parameters we pass an additional int for the length of the parameter */
		if (flag & PROP_DYNAMIC)
			fprintf(f, "\tint %s%s_len;\n", pout ? "*" : "", dparm->prop->identifier);
		
		fprintf(f, "\t%s%s %s%s;\n", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop), ptrstr, dparm->prop->identifier);
	}

	if(has_data) {
		fprintf(f, "\tchar *_data");
		if(func->c_ret) fprintf(f, ", *_retdata");
		fprintf(f, ";\n");
		fprintf(f, "\t\n");
	}

	/* assign self */
	if(func->flag & FUNC_USE_SELF_ID) {
		fprintf(f, "\t_selfid= (struct ID*)_ptr->id.data;\n");
	}
	
	if((func->flag & FUNC_NO_SELF)==0) {
		if(dsrna->dnaname) fprintf(f, "\t_self= (struct %s *)_ptr->data;\n", dsrna->dnaname);
		else fprintf(f, "\t_self= (struct %s *)_ptr->data;\n", srna->identifier);
	}

	if(has_data) {
		fprintf(f, "\t_data= (char *)_parms->data;\n");
	}

	dparm= dfunc->cont.properties.first;
	for(; dparm; dparm= dparm->next) {
		type = dparm->prop->type;
		flag = dparm->prop->flag;
		pout = (flag & PROP_OUTPUT);
		cptr = ((type == PROP_POINTER) && !(flag & PROP_RNAPTR));

		if(dparm->prop==func->c_ret)
			fprintf(f, "\t_retdata= _data;\n");
		else  {
			const char *data_str;
			if (cptr || (flag & PROP_DYNAMIC)) {
				ptrstr= "**";
				valstr= "*";
			}
			else if (type == PROP_POINTER || dparm->prop->arraydimension) {
				ptrstr= "*";
				valstr= "";
			}
			else if (type == PROP_STRING && (flag & PROP_THICK_WRAP)) {
				ptrstr= "";
				valstr= "";
			}
			else {
				ptrstr= "*";
				valstr= "*";
			}

			/* this must be kept in sync with RNA_parameter_length_get_data, we could just call the function directly, but this is faster */
			if (flag & PROP_DYNAMIC) {
				fprintf(f, "\t%s_len= %s((int *)_data);\n", dparm->prop->identifier, pout ? "" : "*");
				data_str= "(&(((char *)_data)[sizeof(void *)]))";
			}
			else {
				data_str= "_data";
			}
			fprintf(f, "\t%s= ", dparm->prop->identifier);

			if (!pout)
				fprintf(f, "%s", valstr);

			fprintf(f, "((%s%s%s)%s);\n", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop), ptrstr, data_str);
		}

		if(dparm->next)
			fprintf(f, "\t_data+= %d;\n", rna_parameter_size_alloc(dparm->prop));
	}

	if(dfunc->call) {
		fprintf(f, "\t\n");
		fprintf(f, "\t");
		if(func->c_ret) fprintf(f, "%s= ", func->c_ret->identifier);
		fprintf(f, "%s(", dfunc->call);

		first= 1;

		if(func->flag & FUNC_USE_SELF_ID) {
			fprintf(f, "_selfid");
			first= 0;
		}

		if((func->flag & FUNC_NO_SELF)==0) {
			if(!first) fprintf(f, ", ");
			fprintf(f, "_self");
			first= 0;
		}

		if(func->flag & FUNC_USE_MAIN) {
			if(!first) fprintf(f, ", ");
			first= 0;
			fprintf(f, "CTX_data_main(C)"); /* may have direct access later */
		}

		if(func->flag & FUNC_USE_CONTEXT) {
			if(!first) fprintf(f, ", ");
			first= 0;
			fprintf(f, "C");
		}

		if(func->flag & FUNC_USE_REPORTS) {
			if(!first) fprintf(f, ", ");
			first= 0;
			fprintf(f, "reports");
		}

		dparm= dfunc->cont.properties.first;
		for(; dparm; dparm= dparm->next) {
			if(dparm->prop==func->c_ret)
				continue;

			if(!first) fprintf(f, ", ");
			first= 0;

			if (dparm->prop->flag & PROP_DYNAMIC)
				fprintf(f, "%s_len, %s", dparm->prop->identifier, dparm->prop->identifier);
			else
				fprintf(f, "%s", dparm->prop->identifier);
		}

		fprintf(f, ");\n");

		if(func->c_ret) {
			dparm= rna_find_parameter_def(func->c_ret);
			ptrstr= (((dparm->prop->type == PROP_POINTER) && !(dparm->prop->flag & PROP_RNAPTR)) || (dparm->prop->arraydimension))? "*": "";
			fprintf(f, "\t*((%s%s%s*)_retdata)= %s;\n", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop), ptrstr, func->c_ret->identifier);
		}
	}

	fprintf(f, "}\n\n");

	dfunc->gencall= funcname;
}

static void rna_auto_types(void)
{
	StructDefRNA *ds;
	PropertyDefRNA *dp;

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next) {
		/* DNA name for Screen is patched in 2.5, we do the reverse here .. */
		if(ds->dnaname && strcmp(ds->dnaname, "Screen") == 0)
			ds->dnaname= "bScreen";

		for(dp=ds->cont.properties.first; dp; dp=dp->next) {
			if(dp->dnastructname && strcmp(dp->dnastructname, "Screen") == 0)
				dp->dnastructname= "bScreen";

			if(dp->dnatype) {
				if(dp->prop->type == PROP_POINTER) {
					PointerPropertyRNA *pprop= (PointerPropertyRNA*)dp->prop;
					StructRNA *type;

					if(!pprop->type && !pprop->get)
						pprop->type= (StructRNA*)rna_find_type(dp->dnatype);

					if(pprop->type) {
						type= rna_find_struct((const char*)pprop->type);
						if(type && (type->flag & STRUCT_ID_REFCOUNT))
							pprop->property.flag |= PROP_ID_REFCOUNT;
					}
				}
				else if(dp->prop->type== PROP_COLLECTION) {
					CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)dp->prop;

					if(!cprop->item_type && !cprop->get && strcmp(dp->dnatype, "ListBase")==0)
						cprop->item_type= (StructRNA*)rna_find_type(dp->dnatype);
				}
			}
		}
	}
}

static void rna_sort(BlenderRNA *brna)
{
	StructDefRNA *ds;
	StructRNA *srna;

	rna_sortlist(&brna->structs, cmp_struct);
	rna_sortlist(&DefRNA.structs, cmp_def_struct);

	for(srna=brna->structs.first; srna; srna=srna->cont.next)
		rna_sortlist(&srna->cont.properties, cmp_property);

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next)
		rna_sortlist(&ds->cont.properties, cmp_def_property);
}

static const char *rna_property_structname(PropertyType type)
{
	switch(type) {
		case PROP_BOOLEAN: return "BoolPropertyRNA";
		case PROP_INT: return "IntPropertyRNA";
		case PROP_FLOAT: return "FloatPropertyRNA";
		case PROP_STRING: return "StringPropertyRNA";
		case PROP_ENUM: return "EnumPropertyRNA";
		case PROP_POINTER: return "PointerPropertyRNA";
		case PROP_COLLECTION: return "CollectionPropertyRNA";
		default: return "UnknownPropertyRNA";
	}
}

static const char *rna_property_subtypename(PropertySubType type)
{
	switch(type) {
		case PROP_NONE: return "PROP_NONE";
		case PROP_FILEPATH: return "PROP_FILEPATH";
		case PROP_FILENAME: return "PROP_FILENAME";
		case PROP_DIRPATH: return "PROP_DIRPATH";
		case PROP_BYTESTRING: return "PROP_BYTESTRING";
		case PROP_TRANSLATE: return "PROP_TRANSLATE";
		case PROP_UNSIGNED: return "PROP_UNSIGNED";
		case PROP_PERCENTAGE: return "PROP_PERCENTAGE";
		case PROP_FACTOR: return "PROP_FACTOR";
		case PROP_ANGLE: return "PROP_ANGLE";
		case PROP_TIME: return "PROP_TIME";
		case PROP_DISTANCE: return "PROP_DISTANCE";
		case PROP_COLOR: return "PROP_COLOR";
		case PROP_TRANSLATION: return "PROP_TRANSLATION";
		case PROP_DIRECTION: return "PROP_DIRECTION";
		case PROP_MATRIX: return "PROP_MATRIX";
		case PROP_EULER: return "PROP_EULER";
		case PROP_QUATERNION: return "PROP_QUATERNION";
		case PROP_AXISANGLE: return "PROP_AXISANGLE";
		case PROP_VELOCITY: return "PROP_VELOCITY";
		case PROP_ACCELERATION: return "PROP_ACCELERATION";
		case PROP_XYZ: return "PROP_XYZ";
		case PROP_COLOR_GAMMA: return "PROP_COLOR_GAMMA";
		case PROP_COORDS: return "PROP_COORDS";
		case PROP_LAYER: return "PROP_LAYER";
		case PROP_LAYER_MEMBER: return "PROP_LAYER_MEMBER";
		default: {
			/* incase we dont have a type preset that includes the subtype */
			if(RNA_SUBTYPE_UNIT(type)) {
				return rna_property_subtypename(type & ~RNA_SUBTYPE_UNIT(type));
			}
			else {
				return "PROP_SUBTYPE_UNKNOWN";
			}
		}
	}
}

static const char *rna_property_subtype_unit(PropertySubType type)
{
	switch(RNA_SUBTYPE_UNIT(type)) {
		case PROP_UNIT_NONE:		return "PROP_UNIT_NONE";
		case PROP_UNIT_LENGTH:		return "PROP_UNIT_LENGTH";
		case PROP_UNIT_AREA:		return "PROP_UNIT_AREA";
		case PROP_UNIT_VOLUME:		return "PROP_UNIT_VOLUME";
		case PROP_UNIT_MASS:		return "PROP_UNIT_MASS";
		case PROP_UNIT_ROTATION:	return "PROP_UNIT_ROTATION";
		case PROP_UNIT_TIME:		return "PROP_UNIT_TIME";
		case PROP_UNIT_VELOCITY:	return "PROP_UNIT_VELOCITY";
		case PROP_UNIT_ACCELERATION:return "PROP_UNIT_ACCELERATION";
		default:					return "PROP_UNIT_UNKNOWN";
	}
}

static void rna_generate_prototypes(BlenderRNA *brna, FILE *f)
{
	StructRNA *srna;

	for(srna=brna->structs.first; srna; srna=srna->cont.next)
		fprintf(f, "extern StructRNA RNA_%s;\n", srna->identifier);
	fprintf(f, "\n");
}

static void rna_generate_blender(BlenderRNA *brna, FILE *f)
{
	StructRNA *srna;

	fprintf(f, "BlenderRNA BLENDER_RNA = {");

	srna= brna->structs.first;
	if(srna) fprintf(f, "{&RNA_%s, ", srna->identifier);
	else fprintf(f, "{NULL, ");

	srna= brna->structs.last;
	if(srna) fprintf(f, "&RNA_%s}", srna->identifier);
	else fprintf(f, "NULL}");

	fprintf(f, "};\n\n");
}

static void rna_generate_property_prototypes(BlenderRNA *brna, StructRNA *srna, FILE *f)
{
	PropertyRNA *prop;
	StructRNA *base;

	base= srna->base;
	while (base) {
		fprintf(f, "\n");
		for(prop=base->cont.properties.first; prop; prop=prop->next)
			fprintf(f, "%s%s rna_%s_%s;\n", "extern ", rna_property_structname(prop->type), base->identifier, prop->identifier);
		base= base->base;
	}

	if(srna->cont.properties.first)
		fprintf(f, "\n");

	for(prop=srna->cont.properties.first; prop; prop=prop->next)
		fprintf(f, "%s%s rna_%s_%s;\n", (prop->flag & PROP_EXPORT)? "": "", rna_property_structname(prop->type), srna->identifier, prop->identifier);
	fprintf(f, "\n");
}

static void rna_generate_parameter_prototypes(BlenderRNA *brna, StructRNA *srna, FunctionRNA *func, FILE *f)
{
	PropertyRNA *parm;

	for(parm= func->cont.properties.first; parm; parm= parm->next)
		fprintf(f, "%s%s rna_%s_%s_%s;\n", "extern ", rna_property_structname(parm->type), srna->identifier, func->identifier, parm->identifier);

	if(func->cont.properties.first)
		fprintf(f, "\n");
}

static void rna_generate_function_prototypes(BlenderRNA *brna, StructRNA *srna, FILE *f)
{
	FunctionRNA *func;
	StructRNA *base;

	base= srna->base;
	while (base) {
		for(func= base->functions.first; func; func= func->cont.next) {
			fprintf(f, "%s%s rna_%s_%s_func;\n", "extern ", "FunctionRNA", base->identifier, func->identifier);
			rna_generate_parameter_prototypes(brna, base, func, f);
		}

		if(base->functions.first)
			fprintf(f, "\n");

		base= base->base;
	}

	for(func= srna->functions.first; func; func= func->cont.next) {
		fprintf(f, "%s%s rna_%s_%s_func;\n", "extern ", "FunctionRNA", srna->identifier, func->identifier);
		rna_generate_parameter_prototypes(brna, srna, func, f);
	}

	if(srna->functions.first)
		fprintf(f, "\n");
}

static void rna_generate_static_parameter_prototypes(BlenderRNA *brna, StructRNA *srna, FunctionDefRNA *dfunc, FILE *f)
{
	FunctionRNA *func;
	PropertyDefRNA *dparm;
	StructDefRNA *dsrna;
	PropertyType type;
	int flag, pout, cptr, first;
	const char *ptrstr;

	dsrna= rna_find_struct_def(srna);
	func= dfunc->func;

	/* return type */
	for(dparm= dfunc->cont.properties.first; dparm; dparm= dparm->next) {
		if(dparm->prop==func->c_ret) {
			if(dparm->prop->arraydimension)
				fprintf(f, "XXX no array return types yet"); /* XXX not supported */
			else if(dparm->prop->type == PROP_POINTER && !(dparm->prop->flag & PROP_RNAPTR))
				fprintf(f, "%s%s *", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop));
			else
				fprintf(f, "%s%s ", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop));

			break;
		}
	}

	/* void if nothing to return */
	if(!dparm)
		fprintf(f, "void ");

	/* function name */
	fprintf(f, "%s(", dfunc->call);

	first= 1;

	/* self, context and reports parameters */
	if(func->flag & FUNC_USE_SELF_ID) {
		fprintf(f, "struct ID *_selfid");
		first= 0;		
	}
	
	if((func->flag & FUNC_NO_SELF)==0) {
		if(!first) fprintf(f, ", ");
		if(dsrna->dnaname) fprintf(f, "struct %s *_self", dsrna->dnaname);
		else fprintf(f, "struct %s *_self", srna->identifier);
		first= 0;
	}

	if(func->flag & FUNC_USE_MAIN) {
		if(!first) fprintf(f, ", ");
		first= 0;
		fprintf(f, "Main *bmain");
	}

	if(func->flag & FUNC_USE_CONTEXT) {
		if(!first) fprintf(f, ", ");
		first= 0;
		fprintf(f, "bContext *C");
	}

	if(func->flag & FUNC_USE_REPORTS) {
		if(!first) fprintf(f, ", ");
		first= 0;
		fprintf(f, "ReportList *reports");
	}

	/* defined parameters */
	for(dparm= dfunc->cont.properties.first; dparm; dparm= dparm->next) {
		type = dparm->prop->type;
		flag = dparm->prop->flag;
		pout = (flag & PROP_OUTPUT);
		cptr = ((type == PROP_POINTER) && !(flag & PROP_RNAPTR));

		if(dparm->prop==func->c_ret)
			continue;

		if (cptr || (flag & PROP_DYNAMIC))
			ptrstr= pout ? "**" : "*";
		else if (type == PROP_POINTER || dparm->prop->arraydimension)
			ptrstr= "*";
		else if (type == PROP_STRING && (flag & PROP_THICK_WRAP))
			ptrstr= "";
		else
			ptrstr= pout ? "*" : "";

		if(!first) fprintf(f, ", ");
		first= 0;

		if (flag & PROP_DYNAMIC)
			fprintf(f, "int %s%s_len, ", pout ? "*" : "", dparm->prop->identifier);

		if(!(flag & PROP_DYNAMIC) && dparm->prop->arraydimension)
			fprintf(f, "%s%s %s[%u]", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop), dparm->prop->identifier, dparm->prop->totarraylength);
		else
			fprintf(f, "%s%s %s%s", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop), ptrstr, dparm->prop->identifier);

	}

	fprintf(f, ");\n");
}

static void rna_generate_static_function_prototypes(BlenderRNA *brna, StructRNA *srna, FILE *f)
{
	FunctionRNA *func;
	FunctionDefRNA *dfunc;
	int first= 1;

	for(func= srna->functions.first; func; func= func->cont.next) {
		dfunc= rna_find_function_def(func);

		if(dfunc->call) {
			if(first) {
				fprintf(f, "/* Repeated prototypes to detect errors */\n\n");
				first= 0;
			}

			rna_generate_static_parameter_prototypes(brna, srna, dfunc, f);
		}
	}

	fprintf(f, "\n");
}

static void rna_generate_property(FILE *f, StructRNA *srna, const char *nest, PropertyRNA *prop) 
{
	char *strnest= "", *errnest= "";
	int len, freenest= 0;
	
	if(nest != NULL) {
		len= strlen(nest);

		strnest= MEM_mallocN(sizeof(char)*(len+2), "rna_generate_property -> strnest");
		errnest= MEM_mallocN(sizeof(char)*(len+2), "rna_generate_property -> errnest");

		strcpy(strnest, "_"); strcat(strnest, nest);
		strcpy(errnest, "."); strcat(errnest, nest);

		freenest= 1;
	}

	switch(prop->type) {
			case PROP_ENUM: {
				EnumPropertyRNA *eprop= (EnumPropertyRNA*)prop;
				int i, defaultfound= 0, totflag= 0;

				if(eprop->item) {
					fprintf(f, "static EnumPropertyItem rna_%s%s_%s_items[%d] = {\n\t", srna->identifier, strnest, prop->identifier, eprop->totitem+1);

					for(i=0; i<eprop->totitem; i++) {
						fprintf(f, "{%d, ", eprop->item[i].value);
						rna_print_c_string(f, eprop->item[i].identifier); fprintf(f, ", ");
						fprintf(f, "%d, ", eprop->item[i].icon);
						rna_print_c_string(f, eprop->item[i].name); fprintf(f, ", ");
						rna_print_c_string(f, eprop->item[i].description); fprintf(f, "},\n\t");

						if(eprop->item[i].identifier[0]) {
							if(prop->flag & PROP_ENUM_FLAG) {
								totflag |= eprop->item[i].value;
							}
							else {
								if(eprop->defaultvalue == eprop->item[i].value) {
									defaultfound= 1;
								}
							}
						}
					}

					fprintf(f, "{0, NULL, 0, NULL, NULL}\n};\n\n");

					if(prop->flag & PROP_ENUM_FLAG) {
						if(eprop->defaultvalue & ~totflag) {
							fprintf(stderr, "%s: %s%s.%s, enum default includes unused bits (%d).\n",
							        __func__, srna->identifier, errnest, prop->identifier, eprop->defaultvalue & ~totflag);
							DefRNA.error= 1;
						}
					}
					else {
						if(!defaultfound) {
							fprintf(stderr, "%s: %s%s.%s, enum default is not in items.\n",
							        __func__, srna->identifier, errnest, prop->identifier);
							DefRNA.error= 1;
						}
					}
				}
				else {
					fprintf(stderr, "%s: %s%s.%s, enum must have items defined.\n",
					        __func__, srna->identifier, errnest, prop->identifier);
					DefRNA.error= 1;
				}
				break;
			}
			case PROP_BOOLEAN: {
				BoolPropertyRNA *bprop= (BoolPropertyRNA*)prop;
				unsigned int i;

				if(prop->arraydimension && prop->totarraylength) {
					fprintf(f, "static int rna_%s%s_%s_default[%u] = {\n\t", srna->identifier, strnest, prop->identifier, prop->totarraylength);

					for(i=0; i<prop->totarraylength; i++) {
						if(bprop->defaultarray)
							fprintf(f, "%d", bprop->defaultarray[i]);
						else
							fprintf(f, "%d", bprop->defaultvalue);
						if(i != prop->totarraylength-1)
							fprintf(f, ",\n\t");
					}

					fprintf(f, "\n};\n\n");
				}
				break;
			}
			case PROP_INT: {
				IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
				unsigned int i;

				if(prop->arraydimension && prop->totarraylength) {
					fprintf(f, "static int rna_%s%s_%s_default[%u] = {\n\t", srna->identifier, strnest, prop->identifier, prop->totarraylength);

					for(i=0; i<prop->totarraylength; i++) {
						if(iprop->defaultarray)
							fprintf(f, "%d", iprop->defaultarray[i]);
						else
							fprintf(f, "%d", iprop->defaultvalue);
						if(i != prop->totarraylength-1)
							fprintf(f, ",\n\t");
					}

					fprintf(f, "\n};\n\n");
				}
				break;
			}
			case PROP_FLOAT: {
				FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
				unsigned int i;

				if(prop->arraydimension && prop->totarraylength) {
					fprintf(f, "static float rna_%s%s_%s_default[%u] = {\n\t", srna->identifier, strnest, prop->identifier, prop->totarraylength);

					for(i=0; i<prop->totarraylength; i++) {
						if(fprop->defaultarray)
							rna_float_print(f, fprop->defaultarray[i]);
						else
							rna_float_print(f, fprop->defaultvalue);
						if(i != prop->totarraylength-1)
							fprintf(f, ",\n\t");
					}

					fprintf(f, "\n};\n\n");
				}
				break;
			}
			default:
				break;
	}

	fprintf(f, "%s%s rna_%s%s_%s = {\n", (prop->flag & PROP_EXPORT)? "": "", rna_property_structname(prop->type), srna->identifier, strnest, prop->identifier);

	if(prop->next) fprintf(f, "\t{(PropertyRNA*)&rna_%s%s_%s, ", srna->identifier, strnest, prop->next->identifier);
	else fprintf(f, "\t{NULL, ");
	if(prop->prev) fprintf(f, "(PropertyRNA*)&rna_%s%s_%s,\n", srna->identifier, strnest, prop->prev->identifier);
	else fprintf(f, "NULL,\n");
	fprintf(f, "\t%d, ", prop->magic);
	rna_print_c_string(f, prop->identifier);
	fprintf(f, ", %d, ", prop->flag);
	rna_print_c_string(f, prop->name); fprintf(f, ",\n\t");
	rna_print_c_string(f, prop->description); fprintf(f, ",\n\t");
	fprintf(f, "%d,\n", prop->icon);
	rna_print_c_string(f, prop->translation_context); fprintf(f, ",\n\t");
	fprintf(f, "\t%s, %s|%s, %s, %u, {%u, %u, %u}, %u,\n", RNA_property_typename(prop->type), rna_property_subtypename(prop->subtype), rna_property_subtype_unit(prop->subtype), rna_function_string(prop->getlength), prop->arraydimension, prop->arraylength[0], prop->arraylength[1], prop->arraylength[2], prop->totarraylength);
	fprintf(f, "\t%s%s, %d, %s, %s,\n", (prop->flag & PROP_CONTEXT_UPDATE)? "(UpdateFunc)": "", rna_function_string(prop->update), prop->noteflag, rna_function_string(prop->editable), rna_function_string(prop->itemeditable));

	if(prop->flag & PROP_RAW_ACCESS) rna_set_raw_offset(f, srna, prop);
	else fprintf(f, "\t0, -1");

	/* our own type - collections/arrays only */
	if(prop->srna) fprintf(f, ", &RNA_%s", (const char*)prop->srna);
	else fprintf(f, ", NULL");

	fprintf(f, "},\n");

	switch(prop->type) {
			case PROP_BOOLEAN: {
				BoolPropertyRNA *bprop= (BoolPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s, %d, ", rna_function_string(bprop->get), rna_function_string(bprop->set), rna_function_string(bprop->getarray), rna_function_string(bprop->setarray), bprop->defaultvalue);
				if(prop->arraydimension && prop->totarraylength) fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
				else fprintf(f, "NULL\n");
				break;
			}
			case PROP_INT: {
				IntPropertyRNA *iprop= (IntPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s, %s,\n\t", rna_function_string(iprop->get), rna_function_string(iprop->set), rna_function_string(iprop->getarray), rna_function_string(iprop->setarray), rna_function_string(iprop->range));
				rna_int_print(f, iprop->softmin); fprintf(f, ", ");
				rna_int_print(f, iprop->softmax); fprintf(f, ", ");
				rna_int_print(f, iprop->hardmin); fprintf(f, ", ");
				rna_int_print(f, iprop->hardmax); fprintf(f, ", ");
				rna_int_print(f, iprop->step); fprintf(f, ", ");
				rna_int_print(f, iprop->defaultvalue); fprintf(f, ", ");
				if(prop->arraydimension && prop->totarraylength) fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
				else fprintf(f, "NULL\n");
				break;
			}
			case PROP_FLOAT: {
				FloatPropertyRNA *fprop= (FloatPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s, %s, ", rna_function_string(fprop->get), rna_function_string(fprop->set), rna_function_string(fprop->getarray), rna_function_string(fprop->setarray), rna_function_string(fprop->range));
				rna_float_print(f, fprop->softmin); fprintf(f, ", ");
				rna_float_print(f, fprop->softmax); fprintf(f, ", ");
				rna_float_print(f, fprop->hardmin); fprintf(f, ", ");
				rna_float_print(f, fprop->hardmax); fprintf(f, ", ");
				rna_float_print(f, fprop->step); fprintf(f, ", ");
				rna_int_print(f, (int)fprop->precision); fprintf(f, ", ");
				rna_float_print(f, fprop->defaultvalue); fprintf(f, ", ");
				if(prop->arraydimension && prop->totarraylength) fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
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
				fprintf(f, "\t%s, %s, %s, NULL, ", rna_function_string(eprop->get), rna_function_string(eprop->set), rna_function_string(eprop->itemf));
				if(eprop->item)
					fprintf(f, "rna_%s%s_%s_items, ", srna->identifier, strnest, prop->identifier);
				else
					fprintf(f, "NULL, ");
				fprintf(f, "%d, %d\n", eprop->totitem, eprop->defaultvalue);
				break;
			}
			case PROP_POINTER: {
				PointerPropertyRNA *pprop= (PointerPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s,", rna_function_string(pprop->get), rna_function_string(pprop->set), rna_function_string(pprop->typef), rna_function_string(pprop->poll));
				if(pprop->type) fprintf(f, "&RNA_%s\n", (const char*)pprop->type);
				else fprintf(f, "NULL\n");
				break;
			}
			case PROP_COLLECTION: {
				CollectionPropertyRNA *cprop= (CollectionPropertyRNA*)prop;
				fprintf(f, "\t%s, %s, %s, %s, %s, %s, %s, %s, ", rna_function_string(cprop->begin), rna_function_string(cprop->next), rna_function_string(cprop->end), rna_function_string(cprop->get), rna_function_string(cprop->length), rna_function_string(cprop->lookupint), rna_function_string(cprop->lookupstring), rna_function_string(cprop->assignint));
				if(cprop->item_type) fprintf(f, "&RNA_%s\n", (const char*)cprop->item_type);
				else fprintf(f, "NULL\n");
				break;
			}
	}

	fprintf(f, "};\n\n");

	if(freenest) {
		MEM_freeN(strnest);
		MEM_freeN(errnest);
	}
}

static void rna_generate_struct(BlenderRNA *brna, StructRNA *srna, FILE *f)
{
	FunctionRNA *func;
	FunctionDefRNA *dfunc;
	PropertyRNA *prop, *parm;
	StructRNA *base;

	fprintf(f, "/* %s */\n", srna->name);

	for(prop= srna->cont.properties.first; prop; prop= prop->next)
		rna_generate_property(f, srna, NULL, prop);

	for(func= srna->functions.first; func; func= func->cont.next) {
		for(parm= func->cont.properties.first; parm; parm= parm->next)
			rna_generate_property(f, srna, func->identifier, parm);

		fprintf(f, "%s%s rna_%s_%s_func = {\n", "", "FunctionRNA", srna->identifier, func->identifier);

		if(func->cont.next) fprintf(f, "\t{(FunctionRNA*)&rna_%s_%s_func, ", srna->identifier, ((FunctionRNA*)func->cont.next)->identifier);
		else fprintf(f, "\t{NULL, ");
		if(func->cont.prev) fprintf(f, "(FunctionRNA*)&rna_%s_%s_func,\n", srna->identifier, ((FunctionRNA*)func->cont.prev)->identifier);
		else fprintf(f, "NULL,\n");

		fprintf(f, "\tNULL,\n");

		parm= func->cont.properties.first;
		if(parm) fprintf(f, "\t{(PropertyRNA*)&rna_%s_%s_%s, ", srna->identifier, func->identifier, parm->identifier);
		else fprintf(f, "\t{NULL, ");

		parm= func->cont.properties.last;
		if(parm) fprintf(f, "(PropertyRNA*)&rna_%s_%s_%s}},\n", srna->identifier, func->identifier, parm->identifier);
		else fprintf(f, "NULL}},\n");

		fprintf(f, "\t");
		rna_print_c_string(f, func->identifier);
		fprintf(f, ", %d, ", func->flag);
		rna_print_c_string(f, func->description); fprintf(f, ",\n");

		dfunc= rna_find_function_def(func);
		if(dfunc->gencall) fprintf(f, "\t%s,\n", dfunc->gencall);
		else fprintf(f, "\tNULL,\n");

		if(func->c_ret) fprintf(f, "\t(PropertyRNA*)&rna_%s_%s_%s\n", srna->identifier, func->identifier, func->c_ret->identifier);
		else fprintf(f, "\tNULL\n");

		fprintf(f, "};\n");
		fprintf(f, "\n");
	}

	fprintf(f, "StructRNA RNA_%s = {\n", srna->identifier);

	if(srna->cont.next) fprintf(f, "\t{(ContainerRNA *)&RNA_%s, ", ((StructRNA*)srna->cont.next)->identifier);
	else fprintf(f, "\t{NULL, ");
	if(srna->cont.prev) fprintf(f, "(ContainerRNA *)&RNA_%s,\n", ((StructRNA*)srna->cont.prev)->identifier);
	else fprintf(f, "NULL,\n");

	fprintf(f, "\tNULL,\n");

	prop= srna->cont.properties.first;
	if(prop) fprintf(f, "\t{(PropertyRNA*)&rna_%s_%s, ", srna->identifier, prop->identifier);
	else fprintf(f, "\t{NULL, ");

	prop= srna->cont.properties.last;
	if(prop) fprintf(f, "(PropertyRNA*)&rna_%s_%s}},\n", srna->identifier, prop->identifier);
	else fprintf(f, "NULL}},\n");
	fprintf(f, "\t");
	rna_print_c_string(f, srna->identifier);
	fprintf(f, "\t, NULL,NULL\n"); /* PyType - Cant initialize here */
	fprintf(f, ", %d, ", srna->flag);
	rna_print_c_string(f, srna->name);
	fprintf(f, ", ");
	rna_print_c_string(f, srna->description);
	fprintf(f, ",\n\t%d,\n", srna->icon);

	prop= srna->nameproperty;
	if(prop) {
		base= srna;
		while (base->base && base->base->nameproperty==prop)
			base= base->base;

		fprintf(f, "\t(PropertyRNA*)&rna_%s_%s, ", base->identifier, prop->identifier);
	}
	else fprintf(f, "\tNULL, ");

	prop= srna->iteratorproperty;
	base= srna;
	while (base->base && base->base->iteratorproperty==prop)
		base= base->base;
	fprintf(f, "(PropertyRNA*)&rna_%s_rna_properties,\n", base->identifier);

	if(srna->base) fprintf(f, "\t&RNA_%s,\n", srna->base->identifier);
	else fprintf(f, "\tNULL,\n");

	if(srna->nested) fprintf(f, "\t&RNA_%s,\n", srna->nested->identifier);
	else fprintf(f, "\tNULL,\n");

	fprintf(f, "\t%s,\n", rna_function_string(srna->refine));
	fprintf(f, "\t%s,\n", rna_function_string(srna->path));
	fprintf(f, "\t%s,\n", rna_function_string(srna->reg));
	fprintf(f, "\t%s,\n", rna_function_string(srna->unreg));
	fprintf(f, "\t%s,\n", rna_function_string(srna->instance));
	fprintf(f, "\t%s,\n", rna_function_string(srna->idproperties));

	if(srna->reg && !srna->refine) {
		fprintf(stderr, "%s: %s has a register function, must also have refine function.\n",
		        __func__, srna->identifier);
		DefRNA.error= 1;
	}

	func= srna->functions.first;
	if(func) fprintf(f, "\t{(FunctionRNA*)&rna_%s_%s_func, ", srna->identifier, func->identifier);
	else fprintf(f, "\t{NULL, ");

	func= srna->functions.last;
	if(func) fprintf(f, "(FunctionRNA*)&rna_%s_%s_func}\n", srna->identifier, func->identifier);
	else fprintf(f, "NULL}\n");

	fprintf(f, "};\n");

	fprintf(f, "\n");
}

typedef struct RNAProcessItem {
	const char *filename;
	const char *api_filename;
	void (*define)(BlenderRNA *brna);
} RNAProcessItem;

static RNAProcessItem PROCESS_ITEMS[]= {
	{"rna_rna.c", NULL, RNA_def_rna},
	{"rna_ID.c", NULL, RNA_def_ID},
	{"rna_texture.c", "rna_texture_api.c", RNA_def_texture},
	{"rna_action.c", "rna_action_api.c", RNA_def_action},
	{"rna_animation.c", "rna_animation_api.c", RNA_def_animation},
	{"rna_animviz.c", NULL, RNA_def_animviz},
	{"rna_actuator.c", "rna_actuator_api.c", RNA_def_actuator},
	{"rna_armature.c", "rna_armature_api.c", RNA_def_armature},
	{"rna_boid.c", NULL, RNA_def_boid},
	{"rna_brush.c", NULL, RNA_def_brush},
	{"rna_camera.c", "rna_camera_api.c", RNA_def_camera},
	{"rna_cloth.c", NULL, RNA_def_cloth},
	{"rna_color.c", NULL, RNA_def_color},
	{"rna_constraint.c", NULL, RNA_def_constraint},
	{"rna_context.c", NULL, RNA_def_context},
	{"rna_controller.c", "rna_controller_api.c", RNA_def_controller},
	{"rna_curve.c", NULL, RNA_def_curve},
	{"rna_dynamicpaint.c", NULL, RNA_def_dynamic_paint},
	{"rna_fcurve.c", "rna_fcurve_api.c", RNA_def_fcurve},
	{"rna_fluidsim.c", NULL, RNA_def_fluidsim},
	{"rna_gpencil.c", NULL, RNA_def_gpencil},
	{"rna_group.c", NULL, RNA_def_group},
	{"rna_image.c", "rna_image_api.c", RNA_def_image},
	{"rna_key.c", NULL, RNA_def_key},
	{"rna_lamp.c", NULL, RNA_def_lamp},
	{"rna_lattice.c", NULL, RNA_def_lattice},
	{"rna_main.c", "rna_main_api.c", RNA_def_main},
	{"rna_material.c", "rna_material_api.c", RNA_def_material},
	{"rna_mesh.c", "rna_mesh_api.c", RNA_def_mesh},
	{"rna_meta.c", NULL, RNA_def_meta},
	{"rna_modifier.c", NULL, RNA_def_modifier},
	{"rna_nla.c", NULL, RNA_def_nla},
	{"rna_nodetree.c", NULL, RNA_def_nodetree},
	{"rna_object.c", "rna_object_api.c", RNA_def_object},
	{"rna_object_force.c", NULL, RNA_def_object_force},
	{"rna_packedfile.c", NULL, RNA_def_packedfile},
	{"rna_particle.c", NULL, RNA_def_particle},
	{"rna_pose.c", "rna_pose_api.c", RNA_def_pose},
	{"rna_property.c", NULL, RNA_def_gameproperty},
	{"rna_render.c", NULL, RNA_def_render},
	{"rna_scene.c", "rna_scene_api.c", RNA_def_scene},
	{"rna_screen.c", NULL, RNA_def_screen},
	{"rna_sculpt_paint.c", NULL, RNA_def_sculpt_paint},
	{"rna_sensor.c", "rna_sensor_api.c", RNA_def_sensor},
	{"rna_sequencer.c", "rna_sequencer_api.c", RNA_def_sequencer},
	{"rna_smoke.c", NULL, RNA_def_smoke},
	{"rna_space.c", NULL, RNA_def_space},
	{"rna_speaker.c", NULL, RNA_def_speaker},
	{"rna_test.c", NULL, RNA_def_test},
	{"rna_text.c", "rna_text_api.c", RNA_def_text},
	{"rna_timeline.c", NULL, RNA_def_timeline_marker},
	{"rna_sound.c", NULL, RNA_def_sound},
	{"rna_ui.c", "rna_ui_api.c", RNA_def_ui},
	{"rna_userdef.c", NULL, RNA_def_userdef},
	{"rna_vfont.c", NULL, RNA_def_vfont},
	{"rna_wm.c", "rna_wm_api.c", RNA_def_wm},
	{"rna_world.c", NULL, RNA_def_world},	
	{"rna_movieclip.c", NULL, RNA_def_movieclip},
	{"rna_tracking.c", NULL, RNA_def_tracking},
	{NULL, NULL}};

static void rna_generate(BlenderRNA *brna, FILE *f, const char *filename, const char *api_filename)
{
	StructDefRNA *ds;
	PropertyDefRNA *dp;
	FunctionDefRNA *dfunc;
	
	fprintf(f, "\n/* Automatically generated struct definitions for the Data API.\n"
				 "   Do not edit manually, changes will be overwritten.           */\n\n"
				  "#define RNA_RUNTIME\n\n");

	fprintf(f, "#include <float.h>\n");
	fprintf(f, "#include <stdio.h>\n");
	fprintf(f, "#include <limits.h>\n");
	fprintf(f, "#include <string.h>\n\n");
	fprintf(f, "#include <stddef.h>\n\n");

	fprintf(f, "#include \"DNA_ID.h\"\n");
	fprintf(f, "#include \"DNA_scene_types.h\"\n");

	fprintf(f, "#include \"BLI_blenlib.h\"\n\n");
	fprintf(f, "#include \"BLI_utildefines.h\"\n\n");

	fprintf(f, "#include \"BKE_context.h\"\n");
	fprintf(f, "#include \"BKE_library.h\"\n");
	fprintf(f, "#include \"BKE_main.h\"\n");
	fprintf(f, "#include \"BKE_report.h\"\n");

	fprintf(f, "#include \"RNA_define.h\"\n");
	fprintf(f, "#include \"RNA_types.h\"\n");
	fprintf(f, "#include \"rna_internal.h\"\n\n");

	rna_generate_prototypes(brna, f);

	fprintf(f, "#include \"%s\"\n", filename);
	if(api_filename)
		fprintf(f, "#include \"%s\"\n", api_filename);
	fprintf(f, "\n");

	fprintf(f, "/* Autogenerated Functions */\n\n");

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next) {
		if(!filename || ds->filename == filename) {
			rna_generate_property_prototypes(brna, ds->srna, f);
			rna_generate_function_prototypes(brna, ds->srna, f);
		}
	}

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next)
		if(!filename || ds->filename == filename)
			for(dp=ds->cont.properties.first; dp; dp=dp->next)
				rna_def_property_funcs(f, ds->srna, dp);

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next) {
		if(!filename || ds->filename == filename) {
			for(dfunc=ds->functions.first; dfunc; dfunc= dfunc->cont.next)
				rna_def_function_funcs(f, ds, dfunc);

			rna_generate_static_function_prototypes(brna, ds->srna, f);
		}
	}

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next)
		if(!filename || ds->filename == filename)
			rna_generate_struct(brna, ds->srna, f);

	if(strcmp(filename, "rna_ID.c") == 0) {
		/* this is ugly, but we cannot have c files compiled for both
		 * makesrna and blender with some build systems at the moment */
		fprintf(f, "#include \"rna_define.c\"\n\n");

		rna_generate_blender(brna, f);
	}
}

static void rna_generate_header(BlenderRNA *brna, FILE *f)
{
	StructDefRNA *ds;
	PropertyDefRNA *dp;
	StructRNA *srna;

	fprintf(f, "\n#ifndef __RNA_BLENDER_H__\n");
	fprintf(f, "#define __RNA_BLENDER_H__\n\n");

	fprintf(f, "/* Automatically generated function declarations for the Data API.\n"
				 "   Do not edit manually, changes will be overwritten.              */\n\n");

	fprintf(f, "#include \"RNA_types.h\"\n\n");

	fprintf(f, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");

	fprintf(f, "#define FOREACH_BEGIN(property, sptr, itemptr) \\\n");
	fprintf(f, "	{ \\\n");
	fprintf(f, "		CollectionPropertyIterator rna_macro_iter; \\\n");
	fprintf(f, "		for(property##_begin(&rna_macro_iter, sptr); rna_macro_iter.valid; property##_next(&rna_macro_iter)) { \\\n");
	fprintf(f, "			itemptr= rna_macro_iter.ptr;\n\n");

	fprintf(f, "#define FOREACH_END(property) \\\n");
	fprintf(f, "		} \\\n");
	fprintf(f, "		property##_end(&rna_macro_iter); \\\n");
	fprintf(f, "	}\n\n");

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next) {
		srna= ds->srna;

		fprintf(f, "/**************** %s ****************/\n\n", srna->name);

		while(srna) {
			fprintf(f, "extern StructRNA RNA_%s;\n", srna->identifier);
			srna= srna->base;
		}
		fprintf(f, "\n");

		for(dp=ds->cont.properties.first; dp; dp=dp->next)
			rna_def_property_funcs_header(f, ds->srna, dp);
	}

	fprintf(f, "#ifdef __cplusplus\n}\n#endif\n\n");

	fprintf(f, "#endif /* __RNA_BLENDER_H__ */\n\n");
}

static const char *cpp_classes = ""
"\n"
"#include <string>\n"
"\n"
"namespace BL {\n"
"\n"
"#define BOOLEAN_PROPERTY(sname, identifier) \\\n"
"	inline bool sname::identifier(void) { return sname##_##identifier##_get(&ptr)? true: false; }\n"
"\n"
"#define BOOLEAN_ARRAY_PROPERTY(sname, size, identifier) \\\n"
"	inline Array<int,size> sname::identifier(void) \\\n"
"		{ Array<int, size> ar; sname##_##identifier##_get(&ptr, ar.data); return ar; }\n"
"\n"
"#define INT_PROPERTY(sname, identifier) \\\n"
"	inline int sname::identifier(void) { return sname##_##identifier##_get(&ptr); }\n"
"\n"
"#define INT_ARRAY_PROPERTY(sname, size, identifier) \\\n"
"	inline Array<int,size> sname::identifier(void) \\\n"
"		{ Array<int, size> ar; sname##_##identifier##_get(&ptr, ar.data); return ar; }\n"
"\n"
"#define FLOAT_PROPERTY(sname, identifier) \\\n"
"	inline float sname::identifier(void) { return sname##_##identifier##_get(&ptr); }\n"
"\n"
"#define FLOAT_ARRAY_PROPERTY(sname, size, identifier) \\\n"
"	inline Array<float,size> sname::identifier(void) \\\n"
"		{ Array<float, size> ar; sname##_##identifier##_get(&ptr, ar.data); return ar; }\n"
"\n"
"#define ENUM_PROPERTY(type, sname, identifier) \\\n"
"	inline sname::type sname::identifier(void) { return (type)sname##_##identifier##_get(&ptr); }\n"
"\n"
"#define STRING_PROPERTY(sname, identifier) \\\n"
"	inline std::string sname::identifier(void) { \\\n"
"		int len= sname##_##identifier##_length(&ptr); \\\n"
"		std::string str; str.resize(len); \\\n"
"		sname##_##identifier##_get(&ptr, &str[0]); return str; } \\\n"
"\n"
"#define POINTER_PROPERTY(type, sname, identifier) \\\n"
"	inline type sname::identifier(void) { return type(sname##_##identifier##_get(&ptr)); }\n"
"\n"
"#define COLLECTION_PROPERTY(type, sname, identifier) \\\n"
"	typedef CollectionIterator<type, sname##_##identifier##_begin, \\\n"
"		sname##_##identifier##_next, sname##_##identifier##_end> identifier##_iterator; \\\n"
"	Collection<sname, type, sname##_##identifier##_begin, \\\n"
"		sname##_##identifier##_next, sname##_##identifier##_end> identifier;\n"
"\n"
"class Pointer {\n"
"public:\n"
"	Pointer(const PointerRNA& p) : ptr(p) { }\n"
"	operator const PointerRNA&() { return ptr; }\n"
"	bool is_a(StructRNA *type) { return RNA_struct_is_a(ptr.type, type)? true: false; }\n"
"	operator void*() { return ptr.data; }\n"
"	operator bool() { return ptr.data != NULL; }\n"
"\n"
"	PointerRNA ptr;\n"
"};\n"
"\n"
"\n"
"template<typename T, int Tsize>\n"
"class Array {\n"
"public:\n"
"	T data[Tsize];\n"
"\n"
"   Array() {}\n"
"	Array(const Array<T, Tsize>& other) { memcpy(data, other.data, sizeof(T)*Tsize); }\n"
"	const Array<T, Tsize>& operator=(const Array<T, Tsize>& other) { memcpy(data, other.data, sizeof(T)*Tsize); return *this; }\n"
"\n"
"	operator T*() { return data; }\n"
"};\n"
"\n"
"typedef void (*TBeginFunc)(CollectionPropertyIterator *iter, PointerRNA *ptr);\n"
"typedef void (*TNextFunc)(CollectionPropertyIterator *iter);\n"
"typedef void (*TEndFunc)(CollectionPropertyIterator *iter);\n"
"\n"
"template<typename T, TBeginFunc Tbegin, TNextFunc Tnext, TEndFunc Tend>\n"
"class CollectionIterator {\n"
"public:\n"
"	CollectionIterator() : t(iter.ptr), init(false) { iter.valid= false; }\n"
"	~CollectionIterator(void) { if(init) Tend(&iter); };\n"
"\n"
"	operator bool(void)\n"
"	{ return iter.valid != 0; }\n"
"	const CollectionIterator<T, Tbegin, Tnext, Tend>& operator++() { Tnext(&iter); t = T(iter.ptr); return *this; }\n"
"\n"
"	T& operator*(void) { return t; }\n"
"	T* operator->(void) { return &t; }\n"
"	bool operator==(const CollectionIterator<T, Tbegin, Tnext, Tend>& other) { return iter.valid == other.iter.valid; }\n"
"	bool operator!=(const CollectionIterator<T, Tbegin, Tnext, Tend>& other) { return iter.valid != other.iter.valid; }\n"
"\n"
"	void begin(const Pointer& ptr)\n"
"	{ if(init) Tend(&iter); Tbegin(&iter, (PointerRNA*)&ptr.ptr); t = T(iter.ptr); init = true; }\n"
"\n"
"private:\n"
"	const CollectionIterator<T, Tbegin, Tnext, Tend>& operator=(const CollectionIterator<T, Tbegin, Tnext, Tend>& copy) {}\n"
""
"	CollectionPropertyIterator iter;\n"
"	T t;\n"
"	bool init;\n"
"};\n"
"\n"
"template<typename Tp, typename T, TBeginFunc Tbegin, TNextFunc Tnext, TEndFunc Tend>\n"
"class Collection {\n"
"public:\n"
"	Collection(const PointerRNA& p) : ptr(p) {}\n"
"\n"
"	void begin(CollectionIterator<T, Tbegin, Tnext, Tend>& iter)\n"
"	{ iter.begin(ptr); }\n"
"	CollectionIterator<T, Tbegin, Tnext, Tend> end()\n"
"	{ return CollectionIterator<T, Tbegin, Tnext, Tend>(); } /* test */ \n"
"\n"
"private:\n"
"	PointerRNA ptr;\n"
"};\n"
"\n";

static void rna_generate_header_cpp(BlenderRNA *brna, FILE *f)
{
	StructDefRNA *ds;
	PropertyDefRNA *dp;
	StructRNA *srna;

	fprintf(f, "\n#ifndef __RNA_BLENDER_CPP_H__\n");
	fprintf(f, "#define __RNA_BLENDER_CPP_H__\n\n");

	fprintf(f, "/* Automatically generated classes for the Data API.\n"
				 "   Do not edit manually, changes will be overwritten. */\n\n");
	
	fprintf(f, "#include \"RNA_blender.h\"\n");
	fprintf(f, "#include \"RNA_types.h\"\n");

	fprintf(f, "%s", cpp_classes);

	fprintf(f, "/**************** Declarations ****************/\n\n");

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next)
		fprintf(f, "class %s;\n", ds->srna->identifier);
	fprintf(f, "\n");

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next) {
		srna= ds->srna;

		fprintf(f, "/**************** %s ****************/\n\n", srna->name);

		fprintf(f, "class %s : public %s {\n", srna->identifier, (srna->base)? srna->base->identifier: "Pointer");
		fprintf(f, "public:\n");
		fprintf(f, "\t%s(const PointerRNA& ptr) :\n\t\t%s(ptr)", srna->identifier, (srna->base)? srna->base->identifier: "Pointer");
		for(dp=ds->cont.properties.first; dp; dp=dp->next)
			if(!(dp->prop->flag & (PROP_IDPROPERTY|PROP_BUILTIN)))
				if(dp->prop->type == PROP_COLLECTION)
					fprintf(f, ",\n\t\t%s(ptr)", dp->prop->identifier);
		fprintf(f, "\n\t\t{}\n\n");

		for(dp=ds->cont.properties.first; dp; dp=dp->next)
			rna_def_property_funcs_header_cpp(f, ds->srna, dp);
		fprintf(f, "};\n\n");
	}


	fprintf(f, "/**************** Implementation ****************/\n");

	for(ds=DefRNA.structs.first; ds; ds=ds->cont.next) {
		for(dp=ds->cont.properties.first; dp; dp=dp->next)
			rna_def_property_funcs_impl_cpp(f, ds->srna, dp);

		fprintf(f, "\n");
	}

	fprintf(f, "}\n\n#endif /* __RNA_BLENDER_CPP_H__ */\n\n");
}

static void make_bad_file(const char *file, int line)
{
	FILE *fp= fopen(file, "w");
	fprintf(fp, "#error \"Error! can't make correct RNA file from %s:%d, STUPID!\"\n", __FILE__, line);
	fclose(fp);
}

static int rna_preprocess(const char *outfile)
{
	BlenderRNA *brna;
	StructDefRNA *ds;
	FILE *file;
	char deffile[4096];
	int i, status;
	const char *deps[3]; /* expand as needed */

	/* define rna */
	brna= RNA_create();

	for(i=0; PROCESS_ITEMS[i].filename; i++) {
		if(PROCESS_ITEMS[i].define) {
			PROCESS_ITEMS[i].define(brna);

			for(ds=DefRNA.structs.first; ds; ds=ds->cont.next)
				if(!ds->filename)
					ds->filename= PROCESS_ITEMS[i].filename;
		}
	}

	rna_auto_types();


	/* create RNA_blender_cpp.h */
	strcpy(deffile, outfile);
	strcat(deffile, "RNA_blender_cpp.h" TMP_EXT);

	status= (DefRNA.error != 0);

	if(status) {
		make_bad_file(deffile, __LINE__);
	}
	else {
		file = fopen(deffile, "w");

		if(!file) {
			fprintf(stderr, "Unable to open file: %s\n", deffile);
			status = 1;
		}
		else {
			rna_generate_header_cpp(brna, file);
			fclose(file);
			status= (DefRNA.error != 0);
		}
	}

	replace_if_different(deffile, NULL);

	rna_sort(brna);

	/* create rna_gen_*.c files */
	for(i=0; PROCESS_ITEMS[i].filename; i++) {
		strcpy(deffile, outfile);
		strcat(deffile, PROCESS_ITEMS[i].filename);
		deffile[strlen(deffile)-2] = '\0';
		strcat(deffile, "_gen.c" TMP_EXT);

		if(status) {
			make_bad_file(deffile, __LINE__);
		}
		else {
			file = fopen(deffile, "w");

			if(!file) {
				fprintf(stderr, "Unable to open file: %s\n", deffile);
				status = 1;
			}
			else {
				rna_generate(brna, file, PROCESS_ITEMS[i].filename, PROCESS_ITEMS[i].api_filename);
				fclose(file);
				status= (DefRNA.error != 0);
			}
		}

		/* avoid unneeded rebuilds */
		deps[0]= PROCESS_ITEMS[i].filename;
		deps[1]= PROCESS_ITEMS[i].api_filename;
		deps[2]= NULL;

		replace_if_different(deffile, deps);
	}

	/* create RNA_blender.h */
	strcpy(deffile, outfile);
	strcat(deffile, "RNA_blender.h" TMP_EXT);

	if(status) {
		make_bad_file(deffile, __LINE__);
	}
	else {
		file = fopen(deffile, "w");

		if(!file) {
			fprintf(stderr, "Unable to open file: %s\n", deffile);
			status = 1;
		}
		else {
			rna_generate_header(brna, file);
			fclose(file);
			status= (DefRNA.error != 0);
		}
	}

	replace_if_different(deffile, NULL);

	/* free RNA */
	RNA_define_free(brna);
	RNA_free(brna);

	return status;
}

static void mem_error_cb(const char *errorStr)
{
	fprintf(stderr, "%s", errorStr);
	fflush(stderr);
}

int main(int argc, char **argv)
{
	int totblock, return_status = 0;

	if(argc<2) {
		fprintf(stderr, "Usage: %s outdirectory/\n", argv[0]);
		return_status = 1;
	}
	else {
		fprintf(stderr, "Running makesrna, program versions %s\n",  RNA_VERSION_DATE);
		makesrna_path= argv[0];
		return_status= rna_preprocess(argv[1]);
	}

	totblock= MEM_get_memory_blocks_in_use();
	if(totblock!=0) {
		fprintf(stderr, "Error Totblock: %d\n",totblock);
		MEM_set_error_callback(mem_error_cb);
		MEM_printmemlist();
	}

	return return_status;
}


