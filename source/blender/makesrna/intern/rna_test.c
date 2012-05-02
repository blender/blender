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
 * Contributor(s): Arystanbek Dyussenov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_test.c
 *  \ingroup RNA
 */


/* Defines a structure with properties used for array manipulation tests in BPY. */

#include <stdlib.h>
#include <string.h>

#include "RNA_define.h"

#include "rna_internal.h"

#define ARRAY_SIZE 3
#define DYNAMIC_ARRAY_SIZE 64
#define MARRAY_DIM [3][4][5]
#define MARRAY_TOTDIM 3
#define MARRAY_DIMSIZE 4, 5
#define MARRAY_SIZE(type) (sizeof(type MARRAY_DIM) / sizeof(type))
#define DYNAMIC_MARRAY_DIM [3][4][5]
#define DYNAMIC_MARRAY_SIZE(type) (sizeof(type DYNAMIC_MARRAY_DIM) / sizeof(type))

#ifdef RNA_RUNTIME

#ifdef UNIT_TEST

#define DEF_VARS(type, prefix)                                \
    static type prefix ## arr[ARRAY_SIZE];                    \
    static type prefix ## darr[DYNAMIC_ARRAY_SIZE];           \
    static int prefix ## darr_len = ARRAY_SIZE;               \
    static type prefix ## marr MARRAY_DIM;                    \
    static type prefix ## dmarr DYNAMIC_MARRAY_DIM;           \
    static int prefix ## dmarr_len = sizeof(prefix ## dmarr); \
    (void)0

#define DEF_GET_SET(type, arr)                                          \
    void rna_Test_ ## arr ## _get(PointerRNA * ptr, type * values)      \
	{                                                                   \
		memcpy(values, arr, sizeof(arr));                               \
	}                                                                   \
                                                                        \
    void rna_Test_ ## arr ## _set(PointerRNA * ptr, const type * values)  \
	{                                                                   \
		memcpy(arr, values, sizeof(arr));                               \
	}                                                                   \
    (void)0

#define DEF_GET_SET_LEN(arr, max)                                       \
    static int rna_Test_ ## arr ## _get_length(PointerRNA * ptr)        \
	{                                                                   \
		return arr ## _len;                                             \
	}                                                                   \
                                                                        \
    static int rna_Test_ ## arr ## _set_length(PointerRNA * ptr, int length) \
	{                                                                   \
		if (length > max)                                               \
			return 0;                                                   \
                                                                        \
		arr ## _len = length;                                           \
                                                                        \
		return 1;                                                       \
	}                                                                   \
    (void)0

DEF_VARS(float, f);
DEF_VARS(int, i);
DEF_VARS(int, b);

DEF_GET_SET(float, farr);
DEF_GET_SET(int, iarr);
DEF_GET_SET(int, barr);

DEF_GET_SET(float, fmarr);
DEF_GET_SET(int, imarr);
DEF_GET_SET(int, bmarr);

DEF_GET_SET(float, fdarr);
DEF_GET_SET_LEN(fdarr, DYNAMIC_ARRAY_SIZE);
DEF_GET_SET(int, idarr);
DEF_GET_SET_LEN(idarr, DYNAMIC_ARRAY_SIZE);
DEF_GET_SET(int, bdarr);
DEF_GET_SET_LEN(bdarr, DYNAMIC_ARRAY_SIZE);

DEF_GET_SET(float, fdmarr);
DEF_GET_SET_LEN(fdmarr, DYNAMIC_MARRAY_SIZE(float));
DEF_GET_SET(int, idmarr);
DEF_GET_SET_LEN(idmarr, DYNAMIC_MARRAY_SIZE(int));
DEF_GET_SET(int, bdmarr);
DEF_GET_SET_LEN(bdmarr, DYNAMIC_MARRAY_SIZE(int));

#endif

#else

void RNA_def_test(BlenderRNA *brna)
{
#ifdef UNIT_TEST
	StructRNA *srna;
	PropertyRNA *prop;
	unsigned short dimsize[] = {MARRAY_DIMSIZE};

	srna = RNA_def_struct(brna, "Test", NULL);
	RNA_def_struct_sdna(srna, "Test");

	prop = RNA_def_float_array(srna, "farr", ARRAY_SIZE, NULL, 0.0f, 0.0f, "farr", "float array", 0.0f, 0.0f);
	RNA_def_property_float_funcs(prop, "rna_Test_farr_get", "rna_Test_farr_set", NULL);

	prop = RNA_def_int_array(srna, "iarr", ARRAY_SIZE, NULL, 0, 0, "iarr", "int array", 0, 0);
	RNA_def_property_int_funcs(prop, "rna_Test_iarr_get", "rna_Test_iarr_set", NULL);

	prop = RNA_def_boolean_array(srna, "barr", ARRAY_SIZE, NULL, "barr", "boolean array");
	RNA_def_property_boolean_funcs(prop, "rna_Test_barr_get", "rna_Test_barr_set");

	/* dynamic arrays */

	prop = RNA_def_float_array(srna, "fdarr", DYNAMIC_ARRAY_SIZE, NULL, 0.0f, 0.0f, "fdarr",
	                           "dynamic float array", 0.0f, 0.0f);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_Test_fdarr_get_length", "rna_Test_fdarr_set_length");
	RNA_def_property_float_funcs(prop, "rna_Test_fdarr_get", "rna_Test_fdarr_set", NULL);

	prop = RNA_def_int_array(srna, "idarr", DYNAMIC_ARRAY_SIZE, NULL, 0, 0, "idarr", "int array", 0, 0);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_Test_idarr_get_length", "rna_Test_idarr_set_length");
	RNA_def_property_int_funcs(prop, "rna_Test_idarr_get", "rna_Test_idarr_set", NULL);
	
	prop = RNA_def_boolean_array(srna, "bdarr", DYNAMIC_ARRAY_SIZE, NULL, "bdarr", "boolean array");
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_Test_bdarr_get_length", "rna_Test_bdarr_set_length");
	RNA_def_property_boolean_funcs(prop, "rna_Test_bdarr_get", "rna_Test_bdarr_set");

	/* multidimensional arrays */

	prop = RNA_def_property(srna, "fmarr", PROP_FLOAT, PROP_NONE);
	RNA_def_property_multidimensional_array(prop, MARRAY_SIZE(float), MARRAY_TOTDIM, dimsize);
	RNA_def_property_float_funcs(prop, "rna_Test_fmarr_get", "rna_Test_fmarr_set", NULL);

	prop = RNA_def_property(srna, "imarr", PROP_INT, PROP_NONE);
	RNA_def_property_multidimensional_array(prop, MARRAY_SIZE(int), MARRAY_TOTDIM, dimsize);
	RNA_def_property_int_funcs(prop, "rna_Test_imarr_get", "rna_Test_imarr_set", NULL);

	prop = RNA_def_property(srna, "bmarr", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_multidimensional_array(prop, MARRAY_SIZE(int), MARRAY_TOTDIM, dimsize);
	RNA_def_property_boolean_funcs(prop, "rna_Test_bmarr_get", "rna_Test_bmarr_set");

	/* dynamic multidimensional arrays */

	prop = RNA_def_property(srna, "fdmarr", PROP_FLOAT, PROP_NONE);
	RNA_def_property_multidimensional_array(prop, DYNAMIC_MARRAY_SIZE(float), MARRAY_TOTDIM, dimsize);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_Test_fdmarr_get_length", "rna_Test_fdmarr_set_length");
	RNA_def_property_float_funcs(prop, "rna_Test_fdmarr_get", "rna_Test_fdmarr_set", NULL);

	prop = RNA_def_property(srna, "idmarr", PROP_INT, PROP_NONE);
	RNA_def_property_multidimensional_array(prop, DYNAMIC_MARRAY_SIZE(int), MARRAY_TOTDIM, dimsize);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_Test_idmarr_get_length", "rna_Test_idmarr_set_length");
	RNA_def_property_int_funcs(prop, "rna_Test_idmarr_get", "rna_Test_idmarr_set", NULL);

	prop = RNA_def_property(srna, "bdmarr", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_multidimensional_array(prop, DYNAMIC_MARRAY_SIZE(int), MARRAY_TOTDIM, dimsize);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_Test_bdmarr_get_length", "rna_Test_bdmarr_set_length");
	RNA_def_property_boolean_funcs(prop, "rna_Test_bdmarr_get", "rna_Test_bdmarr_set");

#endif
}

#endif  /* RNA_RUNTIME */
