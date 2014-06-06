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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * DNA handling
 */

/** \file blender/makesdna/intern/dna_genfile.c
 *  \ingroup DNA
 *
 * Lowest-level functions for decoding the parts of a saved .blend
 * file, including interpretation of its SDNA block and conversion of
 * contents of other parts according to the differences between that
 * SDNA and the SDNA of the current (running) version of Blender.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h" // for MEM_freeN MEM_mallocN MEM_callocN

#include "BLI_utildefines.h"

#ifdef WITH_DNA_GHASH
#  include "BLI_ghash.h"
#endif

#include "DNA_genfile.h"
#include "DNA_sdna_types.h" // for SDNA ;-)

/**
 * \section dna_genfile Overview
 *
 * - please note: no builtin security to detect input of double structs
 * - if you want a struct not to be in DNA file: add two hash marks above it (#<enter>#<enter>)
 *
 * Structure DNA data is added to each blender file and to each executable, this to detect
 * in .blend files new variables in structs, changed array sizes, etc. It's also used for
 * converting endian and pointer size (32-64 bits)
 * As an extra, Python uses a call to detect run-time the contents of a blender struct.
 *
 * Create a structDNA: only needed when one of the input include (.h) files change.
 * File Syntax:
 * <pre>
 *     SDNA (4 bytes) (magic number)
 *     NAME (4 bytes)
 *     <nr> (4 bytes) amount of names (int)
 *     <string>
 *     <string>
 *     ...
 *     ...
 *     TYPE (4 bytes)
 *     <nr> amount of types (int)
 *     <string>
 *     <string>
 *     ...
 *     ...
 *     TLEN (4 bytes)
 *     <len> (short) the lengths of types
 *     <len>
 *     ...
 *     ...
 *     STRC (4 bytes)
 *     <nr> amount of structs (int)
 *     <typenr><nr_of_elems> <typenr><namenr> <typenr><namenr> ...
 *</pre>
 *
 *  **Remember to read/write integer and short aligned!**
 *
 *  While writing a file, the names of a struct is indicated with a type number,
 *  to be found with: ``type = DNA_struct_find_nr(SDNA *, const char *)``
 *  The value of ``type`` corresponds with the the index within the structs array
 *
 *  For the moment: the complete DNA file is included in a .blend file. For
 *  the future we can think of smarter methods, like only included the used
 *  structs. Only needed to keep a file short though...
 *
 * ALLOWED AND TESTED CHANGES IN STRUCTS:
 *  - type change (a char to float will be divided by 255)
 *  - location within a struct (everthing can be randomly mixed up)
 *  - struct within struct (within struct etc), this is recursive
 *  - adding new elements, will be default initialized zero
 *  - removing elements
 *  - change of array sizes
 *  - change of a pointer type: when the name doesn't change the contents is copied
 *
 * NOT YET:
 *  - array (``vec[3]``) to float struct (``vec3f``)
 *
 * DONE:
 *  - endian compatibility
 *  - pointer conversion (32-64 bits)
 *
 * IMPORTANT:
 *  - do not use #defines in structs for array lengths, this cannot be read by the dna functions
 *  - do not use uint, but unsigned int instead, ushort and ulong are allowed
 *  - only use a long in Blender if you want this to be the size of a pointer. so it is
 *    32 bits or 64 bits, dependent at the cpu architecture
 *  - chars are always unsigned
 *  - alignment of variables has to be done in such a way, that any system does
 *    not create 'padding' (gaps) in structures. So make sure that:
 *    - short: 2 aligned
 *    - int: 4 aligned
 *    - float: 4 aligned
 *    - double: 8 aligned
 *    - long: 8 aligned
 *    - struct: 8 aligned
 *  - the sdna functions have several error prints builtin, always check blender running from a console.
 *
 */

/* ************************* ENDIAN STUFF ********************** */

/**
 * converts a short between big/little endian.
 */
static short le_short(short temp)
{
	short new;
	char *rt = (char *)&temp, *rtn = (char *)&new;

	rtn[0] = rt[1];
	rtn[1] = rt[0];

	return new;
}

/**
 * converts an int between big/little endian.
 */
static int le_int(int temp)
{
	int new;
	char *rt = (char *)&temp, *rtn = (char *)&new;

	rtn[0] = rt[3];
	rtn[1] = rt[2];
	rtn[2] = rt[1];
	rtn[3] = rt[0];

	return new;
}


/* ************************* MAKE DNA ********************** */

/* allowed duplicate code from makesdna.c */

/**
 * parses the "[n]" on the end of an array name and returns the number of array elements n.
 */
int DNA_elem_array_size(const char *str)
{
	int a, mul = 1;
	const char *cp = NULL;

	for (a = 0; str[a]; a++) {
		if (str[a] == '[') {
			cp = &(str[a + 1]);
		}
		else if (str[a] == ']' && cp) {
			mul *= atoi(cp);
		}
	}

	return mul;
}

/* ************************* END MAKE DNA ********************** */

/* ************************* DIV ********************** */

void DNA_sdna_free(SDNA *sdna)
{
	MEM_freeN(sdna->data);
	MEM_freeN((void *)sdna->names);
	MEM_freeN(sdna->types);
	MEM_freeN(sdna->structs);

#ifdef WITH_DNA_GHASH
	BLI_ghash_free(sdna->structs_map, NULL, NULL);
#endif

	MEM_freeN(sdna);
}

/**
 * Return true if the name indicates a pointer of some kind.
 */
static bool ispointer(const char *name)
{
	/* check if pointer or function pointer */
	return (name[0] == '*' || (name[0] == '(' && name[1] == '*'));
}

/**
 * Returns the size of struct fields of the specified type and name.
 *
 * \param type  Index into sdna->types/typelens
 * \param name  Index into sdna->names,
 * needed to extract possible pointer/array information.
 */
static int elementsize(const SDNA *sdna, short type, short name)
{
	int mul, namelen, len;
	const char *cp;
	
	cp = sdna->names[name];
	len = 0;
	
	namelen = strlen(cp);
	/* is it a pointer or function pointer? */
	if (ispointer(cp)) {
		/* has the name an extra length? (array) */
		mul = 1;
		if (cp[namelen - 1] == ']') {
			mul = DNA_elem_array_size(cp);
		}
		
		len = sdna->pointerlen * mul;
	}
	else if (sdna->typelens[type]) {
		/* has the name an extra length? (array) */
		mul = 1;
		if (cp[namelen - 1] == ']') {
			mul = DNA_elem_array_size(cp);
		}
		
		len = mul * sdna->typelens[type];
		
	}
	
	return len;
}

#if 0
static void printstruct(SDNA *sdna, short strnr)
{
	/* is for debug */
	int b, nr;
	short *sp;
	
	sp = sdna->structs[strnr];
	
	printf("struct %s\n", sdna->types[sp[0]]);
	nr = sp[1];
	sp += 2;
	
	for (b = 0; b < nr; b++, sp += 2) {
		printf("   %s %s\n", sdna->types[sp[0]], sdna->names[sp[1]]);
	}
}
#endif

/**
 * Returns a pointer to the start of the struct info for the struct with the specified name.
 */
static short *findstruct_name(SDNA *sdna, const char *str)
{
	int a;
	short *sp = NULL;


	for (a = 0; a < sdna->nr_structs; a++) {

		sp = sdna->structs[a];
		
		if (strcmp(sdna->types[sp[0]], str) == 0) {
			return sp;
		}
	}
	
	return NULL;
}

/**
 * Returns the index of the struct info for the struct with the specified name.
 */
int DNA_struct_find_nr(SDNA *sdna, const char *str)
{
	const short *sp = NULL;

	if (sdna->lastfind < sdna->nr_structs) {
		sp = sdna->structs[sdna->lastfind];
		if (strcmp(sdna->types[sp[0]], str) == 0) {
			return sdna->lastfind;
		}
	}

#ifdef WITH_DNA_GHASH
	return (intptr_t)BLI_ghash_lookup(sdna->structs_map, str) - 1;
#else
	{
		int a;

		for (a = 0; a < sdna->nr_structs; a++) {

			sp = sdna->structs[a];

			if (strcmp(sdna->types[sp[0]], str) == 0) {
				sdna->lastfind = a;
				return a;
			}
		}
	}
	return -1;
#endif
}

/* ************************* END DIV ********************** */

/* ************************* READ DNA ********************** */

/**
 * In sdna->data the data, now we convert that to something understandable
 */
static void init_structDNA(SDNA *sdna, bool do_endian_swap)
{
	int *data, *verg, gravity_fix = -1;
	intptr_t nr;
	short *sp;
	char str[8], *cp;
	
	verg = (int *)str;
	data = (int *)sdna->data;

	strcpy(str, "SDNA");
	if (*data == *verg) {
	
		data++;
		
		/* load names array */
		strcpy(str, "NAME");
		if (*data == *verg) {
			data++;
			
			if (do_endian_swap) sdna->nr_names = le_int(*data);
			else sdna->nr_names = *data;
			
			data++;
			sdna->names = MEM_callocN(sizeof(void *) * sdna->nr_names, "sdnanames");
		}
		else {
			printf("NAME error in SDNA file\n");
			return;
		}
		
		nr = 0;
		cp = (char *)data;
		while (nr < sdna->nr_names) {
			sdna->names[nr] = cp;

			/* "float gravity [3]" was parsed wrong giving both "gravity" and
			 * "[3]"  members. we rename "[3]", and later set the type of
			 * "gravity" to "void" so the offsets work out correct */
			if (*cp == '[' && strcmp(cp, "[3]") == 0) {
				if (nr && strcmp(sdna->names[nr - 1], "Cvi") == 0) {
					sdna->names[nr] = "gravity[3]";
					gravity_fix = nr;
				}
			}

			while (*cp) cp++;
			cp++;
			nr++;
		}
		nr = (intptr_t)cp;       /* prevent BUS error */
		nr = (nr + 3) & ~3;
		cp = (char *)nr;
		
		/* load type names array */
		data = (int *)cp;
		strcpy(str, "TYPE");
		if (*data == *verg) {
			data++;
			
			if (do_endian_swap) sdna->nr_types = le_int(*data);
			else sdna->nr_types = *data;
			
			data++;
			sdna->types = MEM_callocN(sizeof(void *) * sdna->nr_types, "sdnatypes");
		}
		else {
			printf("TYPE error in SDNA file\n");
			return;
		}
		
		nr = 0;
		cp = (char *)data;
		while (nr < sdna->nr_types) {
			sdna->types[nr] = cp;
			
			/* this is a patch, to change struct names without a conflict with SDNA */
			/* be careful to use it, in this case for a system-struct (opengl/X) */
			
			if (*cp == 'b') {
				/* struct Screen was already used by X, 'bScreen' replaces the old IrisGL 'Screen' struct */
				if (strcmp("bScreen", cp) == 0) sdna->types[nr] = cp + 1;
			}
			
			while (*cp) cp++;
			cp++;
			nr++;
		}
		nr = (intptr_t)cp;       /* prevent BUS error */
		nr = (nr + 3) & ~3;
		cp = (char *)nr;
		
		/* load typelen array */
		data = (int *)cp;
		strcpy(str, "TLEN");
		if (*data == *verg) {
			data++;
			sp = (short *)data;
			sdna->typelens = sp;
			
			if (do_endian_swap) {
				short a, *spo = sp;
				
				a = sdna->nr_types;
				while (a--) {
					spo[0] = le_short(spo[0]);
					spo++;
				}
			}
			
			sp += sdna->nr_types;
		}
		else {
			printf("TLEN error in SDNA file\n");
			return;
		}
		if (sdna->nr_types & 1) sp++;   /* prevent BUS error */

		/* load struct array */
		data = (int *)sp;
		strcpy(str, "STRC");
		if (*data == *verg) {
			data++;
			
			if (do_endian_swap) sdna->nr_structs = le_int(*data);
			else sdna->nr_structs = *data;
			
			data++;
			sdna->structs = MEM_callocN(sizeof(void *) * sdna->nr_structs, "sdnastrcs");
		}
		else {
			printf("STRC error in SDNA file\n");
			return;
		}
		
		nr = 0;
		sp = (short *)data;
		while (nr < sdna->nr_structs) {
			sdna->structs[nr] = sp;
			
			if (do_endian_swap) {
				short a;
				
				sp[0] = le_short(sp[0]);
				sp[1] = le_short(sp[1]);
				
				a = sp[1];
				sp += 2;
				while (a--) {
					sp[0] = le_short(sp[0]);
					sp[1] = le_short(sp[1]);
					sp += 2;
				}
			}
			else {
				sp += 2 * sp[1] + 2;
			}
			
			nr++;
		}

		/* finally pointerlen: use struct ListBase to test it, never change the size of it! */
		sp = findstruct_name(sdna, "ListBase");
		/* weird; i have no memory of that... I think I used sizeof(void *) before... (ton) */
		
		sdna->pointerlen = sdna->typelens[sp[0]] / 2;

		if (sp[1] != 2 || (sdna->pointerlen != 4 && sdna->pointerlen != 8)) {
			printf("ListBase struct error! Needs it to calculate pointerize.\n");
			exit(1);
			/* well, at least sizeof(ListBase) is error proof! (ton) */
		}
		
		/* second part of gravity problem, setting "gravity" type to void */
		if (gravity_fix > -1) {
			for (nr = 0; nr < sdna->nr_structs; nr++) {
				sp = sdna->structs[nr];
				if (strcmp(sdna->types[sp[0]], "ClothSimSettings") == 0)
					sp[10] = SDNA_TYPE_VOID;
			}
		}

#ifdef WITH_DNA_GHASH
		/* create a ghash lookup to speed up */
		sdna->structs_map = BLI_ghash_str_new_ex("init_structDNA gh", sdna->nr_structs);

		for (nr = 0; nr < sdna->nr_structs; nr++) {
			sp = sdna->structs[nr];
			BLI_ghash_insert(sdna->structs_map, (void *)sdna->types[sp[0]], (void *)(nr + 1));
		}
#endif
	}
}

/**
 * Constructs and returns a decoded SDNA structure from the given encoded SDNA data block.
 */
SDNA *DNA_sdna_from_data(const void *data, const int datalen, bool do_endian_swap)
{
	SDNA *sdna = MEM_mallocN(sizeof(*sdna), "sdna");
	
	sdna->lastfind = 0;

	sdna->datalen = datalen;
	sdna->data = MEM_mallocN(datalen, "sdna_data");
	memcpy(sdna->data, data, datalen);
	
	init_structDNA(sdna, do_endian_swap);
	
	return sdna;
}

/* ******************** END READ DNA ********************** */

/* ******************* HANDLE DNA ***************** */

/**
 * Used by #DNA_struct_get_compareflags (below) to recursively mark all structs
 * containing a field of type structnr as changed between old and current SDNAs.
 */
static void recurs_test_compflags(const SDNA *sdna, char *compflags, int structnr)
{
	int a, b, typenr, elems;
	const short *sp;
	const char *cp;
	
	/* check all structs, test if it's inside another struct */
	sp = sdna->structs[structnr];
	typenr = sp[0];
	
	for (a = 0; a < sdna->nr_structs; a++) {
		if (a != structnr && compflags[a] == 1) {
			sp = sdna->structs[a];
			elems = sp[1];
			sp += 2;
			for (b = 0; b < elems; b++, sp += 2) {
				if (sp[0] == typenr) {
					cp = sdna->names[sp[1]];
					if (!ispointer(cp)) {
						compflags[a] = 2;
						recurs_test_compflags(sdna, compflags, a);
					}
				}
			}
		}
	}
	
}


/**
 * Constructs and returns an array of byte flags with one element for each struct in oldsdna,
 * indicating how it compares to newsdna:
 *
 * flag value:
 * - 0  Struct has disappeared (values of this struct type will not be loaded by the current Blender)
 * - 1  Struct is the same (can be loaded with straight memory copy after any necessary endian conversion)
 * - 2  Struct is different in some way (needs to be copied/converted field by field)
 */
char *DNA_struct_get_compareflags(SDNA *oldsdna, SDNA *newsdna)
{
	int a, b;
	const short *sp_old, *sp_new;
	const char *str1, *str2;
	char *compflags;
	
	if (oldsdna->nr_structs == 0) {
		printf("error: file without SDNA\n");
		return NULL;
	}

	compflags = MEM_callocN(oldsdna->nr_structs, "compflags");

	/* we check all structs in 'oldsdna' and compare them with 
	 * the structs in 'newsdna'
	 */
	
	for (a = 0; a < oldsdna->nr_structs; a++) {
		sp_old = oldsdna->structs[a];
		
		/* search for type in cur */
		sp_new = findstruct_name(newsdna, oldsdna->types[sp_old[0]]);
		
		if (sp_new) {
			compflags[a] = 2; /* initial assumption */
			
			/* compare length and amount of elems */
			if (sp_new[1] == sp_old[1]) {
				if (newsdna->typelens[sp_new[0]] == oldsdna->typelens[sp_old[0]]) {

					/* same length, same amount of elems, now per type and name */
					b = sp_old[1];
					sp_old += 2;
					sp_new += 2;
					while (b > 0) {
						str1 = newsdna->types[sp_new[0]];
						str2 = oldsdna->types[sp_old[0]];
						if (strcmp(str1, str2) != 0) break;

						str1 = newsdna->names[sp_new[1]];
						str2 = oldsdna->names[sp_old[1]];
						if (strcmp(str1, str2) != 0) break;

						/* same type and same name, now pointersize */
						if (ispointer(str1)) {
							if (oldsdna->pointerlen != newsdna->pointerlen) break;
						}

						b--;
						sp_old += 2;
						sp_new += 2;
					}
					if (b == 0) compflags[a] = 1; /* no differences found */

				}
			}
			
		}
	}

	/* first struct in util.h is struct Link, this is skipped in compflags (als # 0).
	 * was a bug, and this way dirty patched! Solve this later....
	 */
	compflags[0] = 1;

	/* Because structs can be inside structs, we recursively
	 * set flags when a struct is altered
	 */
	for (a = 0; a < oldsdna->nr_structs; a++) {
		if (compflags[a] == 2) recurs_test_compflags(oldsdna, compflags, a);
	}
	
#if 0
	for (a = 0; a < oldsdna->nr_structs; a++) {
		if (compflags[a] == 2) {
			spold = oldsdna->structs[a];
			printf("changed: %s\n", oldsdna->types[spold[0]]);
		}
	}
#endif

	return compflags;
}

/**
 * Converts the name of a primitive type to its enumeration code.
 */
static eSDNA_Type sdna_type_nr(const char *dna_type)
{
	if     ((strcmp(dna_type, "char") == 0) || (strcmp(dna_type, "const char") == 0))          return SDNA_TYPE_CHAR;
	else if ((strcmp(dna_type, "uchar") == 0) || (strcmp(dna_type, "unsigned char") == 0))     return SDNA_TYPE_UCHAR;
	else if ( strcmp(dna_type, "short") == 0)                                                  return SDNA_TYPE_SHORT;
	else if ((strcmp(dna_type, "ushort") == 0) || (strcmp(dna_type, "unsigned short") == 0))   return SDNA_TYPE_USHORT;
	else if ( strcmp(dna_type, "int") == 0)                                                    return SDNA_TYPE_INT;
	else if ( strcmp(dna_type, "long") == 0)                                                   return SDNA_TYPE_LONG;
	else if ((strcmp(dna_type, "ulong") == 0) || (strcmp(dna_type, "unsigned long") == 0))     return SDNA_TYPE_ULONG;
	else if ( strcmp(dna_type, "float") == 0)                                                  return SDNA_TYPE_FLOAT;
	else if ( strcmp(dna_type, "double") == 0)                                                 return SDNA_TYPE_DOUBLE;
	else if ( strcmp(dna_type, "int64_t") == 0)                                                return SDNA_TYPE_INT64;
	else if ( strcmp(dna_type, "uint64_t") == 0)                                               return SDNA_TYPE_UINT64;
	else                                                                                       return -1; /* invalid! */
}

/**
 * Converts a value of one primitive type to another.
 * Note there is no optimization for the case where otype and ctype are the same:
 * assumption is that caller will handle this case.
 *
 * \param ctype  Name of type to convert to
 * \param otype  Name of type to convert from
 * \param name  Field name to extract array-size information
 * \param curdata  Where to put converted data
 * \param olddata  Data of type otype to convert
 */
static void cast_elem(
        const char *ctype, const char *otype, const char *name,
        char *curdata, const char *olddata)
{
	double val = 0.0;
	int arrlen, curlen = 1, oldlen = 1;

	eSDNA_Type ctypenr, otypenr;

	arrlen = DNA_elem_array_size(name);

	if ( (otypenr = sdna_type_nr(otype)) == -1 ||
	     (ctypenr = sdna_type_nr(ctype)) == -1)
	{
		return;
	}

	/* define lengths */
	oldlen = DNA_elem_type_size(otypenr);
	curlen = DNA_elem_type_size(ctypenr);

	while (arrlen > 0) {
		switch (otypenr) {
			case SDNA_TYPE_CHAR:
				val = *olddata; break;
			case SDNA_TYPE_UCHAR:
				val = *( (unsigned char *)olddata); break;
			case SDNA_TYPE_SHORT:
				val = *( (short *)olddata); break;
			case SDNA_TYPE_USHORT:
				val = *( (unsigned short *)olddata); break;
			case SDNA_TYPE_INT:
				val = *( (int *)olddata); break;
			case SDNA_TYPE_LONG:
				val = *( (int *)olddata); break;
			case SDNA_TYPE_ULONG:
				val = *( (unsigned int *)olddata); break;
			case SDNA_TYPE_FLOAT:
				val = *( (float *)olddata); break;
			case SDNA_TYPE_DOUBLE:
				val = *( (double *)olddata); break;
			case SDNA_TYPE_INT64:
				val = *( (int64_t *)olddata); break;
			case SDNA_TYPE_UINT64:
				val = *( (uint64_t *)olddata); break;
		}
		
		switch (ctypenr) {
			case SDNA_TYPE_CHAR:
				*curdata = val; break;
			case SDNA_TYPE_UCHAR:
				*( (unsigned char *)curdata) = val; break;
			case SDNA_TYPE_SHORT:
				*( (short *)curdata) = val; break;
			case SDNA_TYPE_USHORT:
				*( (unsigned short *)curdata) = val; break;
			case SDNA_TYPE_INT:
				*( (int *)curdata) = val; break;
			case SDNA_TYPE_LONG:
				*( (int *)curdata) = val; break;
			case SDNA_TYPE_ULONG:
				*( (unsigned int *)curdata) = val; break;
			case SDNA_TYPE_FLOAT:
				if (otypenr < 2) val /= 255;
				*( (float *)curdata) = val; break;
			case SDNA_TYPE_DOUBLE:
				if (otypenr < 2) val /= 255;
				*( (double *)curdata) = val; break;
			case SDNA_TYPE_INT64:
				*( (int64_t *)curdata) = val; break;
			case SDNA_TYPE_UINT64:
				*( (uint64_t *)curdata) = val; break;
		}

		olddata += oldlen;
		curdata += curlen;
		arrlen--;
	}
}

/**
 * Converts pointer values between different sizes. These are only used
 * as lookup keys to identify data blocks in the saved .blend file, not
 * as actual in-memory pointers.
 *
 * \param curlen  Pointer length to conver to
 * \param oldlen  Length of pointers in olddata
 * \param name  Field name to extract array-size information
 * \param curdata  Where to put converted data
 * \param olddata  Data to convert
 */
static void cast_pointer(int curlen, int oldlen, const char *name, char *curdata, const char *olddata)
{
	int64_t lval;
	int arrlen;
	
	arrlen = DNA_elem_array_size(name);
	
	while (arrlen > 0) {
	
		if (curlen == oldlen) {
			memcpy(curdata, olddata, curlen);
		}
		else if (curlen == 4 && oldlen == 8) {
			lval = *((int64_t *)olddata);

			/* WARNING: 32-bit Blender trying to load file saved by 64-bit Blender,
			 * pointers may lose uniqueness on truncation! (Hopefully this wont
			 * happen unless/until we ever get to multi-gigabyte .blend files...) */
			*((int *)curdata) = lval >> 3;
		}
		else if (curlen == 8 && oldlen == 4) {
			*((int64_t *)curdata) = *((int *)olddata);
		}
		else {
			/* for debug */
			printf("errpr: illegal pointersize!\n");
		}
		
		olddata += oldlen;
		curdata += curlen;
		arrlen--;

	}
}

/**
 * Equality test on name and oname excluding any array-size suffix.
 */
static int elem_strcmp(const char *name, const char *oname)
{
	int a = 0;
	
	while (1) {
		if (name[a] != oname[a]) return 1;
		if (name[a] == '[' || oname[a] == '[') break;
		if (name[a] == 0 || oname[a] == 0) break;
		a++;
	}
	return 0;
}

/**
 * Returns the address of the data for the specified field within olddata
 * according to the struct format pointed to by old, or NULL if no such
 * field can be found.
 *
 * \param sdna  Old SDNA
 * \param type  Current field type name
 * \param name  Current field name
 * \param old  Pointer to struct information in sdna
 * \param olddata  Struct data
 * \param sppo  Optional place to return pointer to field info in sdna
 * \return Data address.
 */
static char *find_elem(
        const SDNA *sdna,
        const char *type,
        const char *name,
        const short *old,
        char *olddata,
        const short **sppo)
{
	int a, elemcount, len;
	const char *otype, *oname;
	
	/* without arraypart, so names can differ: return old namenr and type */
	
	/* in old is the old struct */
	elemcount = old[1];
	old += 2;
	for (a = 0; a < elemcount; a++, old += 2) {

		otype = sdna->types[old[0]];
		oname = sdna->names[old[1]];

		len = elementsize(sdna, old[0], old[1]);

		if (elem_strcmp(name, oname) == 0) {  /* name equal */
			if (strcmp(type, otype) == 0) {   /* type equal */
				if (sppo) *sppo = old;
				return olddata;
			}
			
			return NULL;
		}
		
		olddata += len;
	}
	return NULL;
}

/**
 * Converts the contents of a single field of a struct, of a non-struct type,
 * from oldsdna to newsdna format.
 *
 * \param newsdna  SDNA of current Blender
 * \param oldsdna  SDNA of Blender that saved file
 * \param type  current field type name
 * \param name  current field name
 * \param curdata  put field data converted to newsdna here
 * \param old  pointer to struct info in oldsdna
 * \param olddata  struct contents laid out according to oldsdna
 */
static void reconstruct_elem(
        const SDNA *newsdna,
        const SDNA *oldsdna,
        const char *type,
        const char *name,
        char *curdata,
        const short *old,
        const char *olddata)
{
	/* rules: test for NAME:
	 *      - name equal:
	 *          - cast type
	 *      - name partially equal (array differs)
	 *          - type equal: memcpy
	 *          - types casten
	 * (nzc 2-4-2001 I want the 'unsigned' bit to be parsed as well. Where
	 * can I force this?)
	 */
	int a, elemcount, len, countpos, oldsize, cursize, mul;
	const char *otype, *oname, *cp;
	
	/* is 'name' an array? */
	cp = name;
	countpos = 0;
	while (*cp && *cp != '[') {
		cp++; countpos++;
	}
	if (*cp != '[') countpos = 0;
	
	/* in old is the old struct */
	elemcount = old[1];
	old += 2;
	for (a = 0; a < elemcount; a++, old += 2) {
		otype = oldsdna->types[old[0]];
		oname = oldsdna->names[old[1]];
		len = elementsize(oldsdna, old[0], old[1]);
		
		if (strcmp(name, oname) == 0) { /* name equal */
			
			if (ispointer(name)) {  /* pointer of functionpointer afhandelen */
				cast_pointer(newsdna->pointerlen, oldsdna->pointerlen, name, curdata, olddata);
			}
			else if (strcmp(type, otype) == 0) {    /* type equal */
				memcpy(curdata, olddata, len);
			}
			else {
				cast_elem(type, otype, name, curdata, olddata);
			}

			return;
		}
		else if (countpos != 0) {  /* name is an array */

			if (oname[countpos] == '[' && strncmp(name, oname, countpos) == 0) {  /* basis equal */
				
				cursize = DNA_elem_array_size(name);
				oldsize = DNA_elem_array_size(oname);

				if (ispointer(name)) {  /* handle pointer or functionpointer */
					cast_pointer(newsdna->pointerlen, oldsdna->pointerlen,
					             cursize > oldsize ? oname : name,
					             curdata, olddata);
				}
				else if (strcmp(type, otype) == 0) {  /* type equal */
					mul = len / oldsize; /* size of single old array element */
					mul *= (cursize < oldsize) ? cursize : oldsize; /* smaller of sizes of old and new arrays */
					memcpy(curdata, olddata, mul);
					
					if (oldsize > cursize && strcmp(type, "char") == 0) {
						/* string had to be truncated, ensure it's still null-terminated */
						curdata[mul - 1] = '\0';
					}
				}
				else {
					cast_elem(type, otype,
					          cursize > oldsize ? oname : name,
					          curdata, olddata);
				}
				return;
			}
		}
		olddata += len;
	}
}

/**
 * Converts the contents of an entire struct from oldsdna to newsdna format.
 *
 * \param newsdna  SDNA of current Blender
 * \param oldsdna  SDNA of Blender that saved file
 * \param compflags
 *
 * Result from DNA_struct_get_compareflags to avoid needless conversions.
 * \param oldSDNAnr  Index of old struct definition in oldsdna
 * \param data  Struct contents laid out according to oldsdna
 * \param curSDNAnr  Index of current struct definition in newsdna
 * \param cur  Where to put converted struct contents
 */
static void reconstruct_struct(
        SDNA *newsdna,
        SDNA *oldsdna,
        const char *compflags,

        int oldSDNAnr,
        char *data,
        int curSDNAnr,
        char *cur)
{
	/* Recursive!
	 * Per element from cur_struct, read data from old_struct.
	 * If element is a struct, call recursive.
	 */
	int a, elemcount, elen, eleno, mul, mulo, firststructtypenr;
	const short *spo, *spc, *sppo;
	const char *type;
	char *cpo, *cpc;
	const char *name, *nameo;

	if (oldSDNAnr == -1) return;
	if (curSDNAnr == -1) return;

	if (compflags[oldSDNAnr] == 1) {        /* if recursive: test for equal */
	
		spo = oldsdna->structs[oldSDNAnr];
		elen = oldsdna->typelens[spo[0]];
		memcpy(cur, data, elen);
		
		return;
	}

	firststructtypenr = *(newsdna->structs[0]);

	spo = oldsdna->structs[oldSDNAnr];
	spc = newsdna->structs[curSDNAnr];

	elemcount = spc[1];

	spc += 2;
	cpc = cur;
	for (a = 0; a < elemcount; a++, spc += 2) {  /* convert each field */
		type = newsdna->types[spc[0]];
		name = newsdna->names[spc[1]];
		
		elen = elementsize(newsdna, spc[0], spc[1]);

		/* test: is type a struct? */
		if (spc[0] >= firststructtypenr && !ispointer(name)) {
			/* struct field type */
			/* where does the old struct data start (and is there an old one?) */
			cpo = find_elem(oldsdna, type, name, spo, data, &sppo);
			
			if (cpo) {
				oldSDNAnr = DNA_struct_find_nr(oldsdna, type);
				curSDNAnr = DNA_struct_find_nr(newsdna, type);
				
				/* array! */
				mul = DNA_elem_array_size(name);
				nameo = oldsdna->names[sppo[1]];
				mulo = DNA_elem_array_size(nameo);
				
				eleno = elementsize(oldsdna, sppo[0], sppo[1]);
				
				elen /= mul;
				eleno /= mulo;
				
				while (mul--) {
					reconstruct_struct(newsdna, oldsdna, compflags, oldSDNAnr, cpo, curSDNAnr, cpc);
					cpo += eleno;
					cpc += elen;
					
					/* new struct array larger than old */
					mulo--;
					if (mulo <= 0) break;
				}
			}
			else {
				cpc += elen;  /* skip field no longer present */
			}
		}
		else {
			/* non-struct field type */
			reconstruct_elem(newsdna, oldsdna, type, name, cpc, spo, data);
			cpc += elen;
		}
	}
}

/**
 * Does endian swapping on the fields of a struct value.
 *
 * \param oldsdna  SDNA of Blender that saved file
 * \param oldSDNAnr  Index of struct info within oldsdna
 * \param data  Struct data
 */
void DNA_struct_switch_endian(SDNA *oldsdna, int oldSDNAnr, char *data)
{
	/* Recursive!
	 * If element is a struct, call recursive.
	 */
	int a, mul, elemcount, elen, elena, firststructtypenr;
	const short *spo, *spc;
	char *cpo, *cur, cval;
	const char *type, *name;

	if (oldSDNAnr == -1) return;
	firststructtypenr = *(oldsdna->structs[0]);
	
	spo = spc = oldsdna->structs[oldSDNAnr];

	elemcount = spo[1];

	spc += 2;
	cur = data;
	
	for (a = 0; a < elemcount; a++, spc += 2) {
		type = oldsdna->types[spc[0]];
		name = oldsdna->names[spc[1]];
		
		/* elementsize = including arraysize */
		elen = elementsize(oldsdna, spc[0], spc[1]);

		/* test: is type a struct? */
		if (spc[0] >= firststructtypenr && !ispointer(name)) {
		  /* struct field type */
			/* where does the old data start (is there one?) */
			cpo = find_elem(oldsdna, type, name, spo, data, NULL);
			if (cpo) {
				oldSDNAnr = DNA_struct_find_nr(oldsdna, type);
				
				mul = DNA_elem_array_size(name);
				elena = elen / mul;

				while (mul--) {
					DNA_struct_switch_endian(oldsdna, oldSDNAnr, cpo);
					cpo += elena;
				}
			}
		}
		else {
		  /* non-struct field type */
			if (ispointer(name)) {
				if (oldsdna->pointerlen == 8) {
					
					mul = DNA_elem_array_size(name);
					cpo = cur;
					while (mul--) {
						cval = cpo[0]; cpo[0] = cpo[7]; cpo[7] = cval;
						cval = cpo[1]; cpo[1] = cpo[6]; cpo[6] = cval;
						cval = cpo[2]; cpo[2] = cpo[5]; cpo[5] = cval;
						cval = cpo[3]; cpo[3] = cpo[4]; cpo[4] = cval;
						
						cpo += 8;
					}
					
				}
			}
			else {
				
				if (spc[0] == SDNA_TYPE_SHORT ||
				    spc[0] == SDNA_TYPE_USHORT)
				{
					
					/* exception: variable called blocktype/ipowin: derived from ID_  */
					bool skip = false;
					if (name[0] == 'b' && name[1] == 'l') {
						if (strcmp(name, "blocktype") == 0) skip = true;
					}
					else if (name[0] == 'i' && name[1] == 'p') {
						if (strcmp(name, "ipowin") == 0) skip = true;
					}
					
					if (skip == false) {
						mul = DNA_elem_array_size(name);
						cpo = cur;
						while (mul--) {
							cval = cpo[0];
							cpo[0] = cpo[1];
							cpo[1] = cval;
							cpo += 2;
						}
					}
				}
				else if ( (spc[0] == SDNA_TYPE_INT    ||
				           spc[0] == SDNA_TYPE_LONG   ||
				           spc[0] == SDNA_TYPE_ULONG  ||
				           spc[0] == SDNA_TYPE_FLOAT))
				{

					mul = DNA_elem_array_size(name);
					cpo = cur;
					while (mul--) {
						cval = cpo[0];
						cpo[0] = cpo[3];
						cpo[3] = cval;
						cval = cpo[1];
						cpo[1] = cpo[2];
						cpo[2] = cval;
						cpo += 4;
					}
				}
				else if ( (spc[0] == SDNA_TYPE_INT64) ||
				          (spc[0] == SDNA_TYPE_UINT64))
				{
					mul = DNA_elem_array_size(name);
					cpo = cur;
					while (mul--) {
						cval = cpo[0]; cpo[0] = cpo[7]; cpo[7] = cval;
						cval = cpo[1]; cpo[1] = cpo[6]; cpo[6] = cval;
						cval = cpo[2]; cpo[2] = cpo[5]; cpo[5] = cval;
						cval = cpo[3]; cpo[3] = cpo[4]; cpo[4] = cval;

						cpo += 8;
					}
				}
				/* FIXME: no conversion for SDNA_TYPE_DOUBLE? */
			}
		}
		cur += elen;
	}
}

/**
 * \param newsdna  SDNA of current Blender
 * \param oldsdna  SDNA of Blender that saved file
 * \param compflags
 *
 * Result from DNA_struct_get_compareflags to avoid needless conversions
 * \param oldSDNAnr  Index of struct info within oldsdna
 * \param blocks  The number of array elements
 * \param data  Array of struct data
 * \return An allocated reconstructed struct
 */
void *DNA_struct_reconstruct(SDNA *newsdna, SDNA *oldsdna, char *compflags, int oldSDNAnr, int blocks, void *data)
{
	int a, curSDNAnr, curlen = 0, oldlen;
	const short *spo, *spc;
	char *cur, *cpc, *cpo;
	const char *type;
	
	/* oldSDNAnr == structnr, we're looking for the corresponding 'cur' number */
	spo = oldsdna->structs[oldSDNAnr];
	type = oldsdna->types[spo[0]];
	oldlen = oldsdna->typelens[spo[0]];
	curSDNAnr = DNA_struct_find_nr(newsdna, type);

	/* init data and alloc */
	if (curSDNAnr != -1) {
		spc = newsdna->structs[curSDNAnr];
		curlen = newsdna->typelens[spc[0]];
	}
	if (curlen == 0) {
		return NULL;
	}

	cur = MEM_callocN(blocks * curlen, "reconstruct");
	cpc = cur;
	cpo = data;
	for (a = 0; a < blocks; a++) {
		reconstruct_struct(newsdna, oldsdna, compflags, oldSDNAnr, cpo, curSDNAnr, cpc);
		cpc += curlen;
		cpo += oldlen;
	}

	return cur;
}

/**
 * Returns the offset of the field with the specified name and type within the specified
 * struct type in sdna.
 */
int DNA_elem_offset(SDNA *sdna, const char *stype, const char *vartype, const char *name)
{
	
	const int SDNAnr = DNA_struct_find_nr(sdna, stype);
	const short * const spo = sdna->structs[SDNAnr];
	const char * const cp = find_elem(sdna, vartype, name, spo, NULL, NULL);
	BLI_assert(SDNAnr != -1);
	return (int)((intptr_t)cp);
}

bool DNA_struct_elem_find(SDNA *sdna, const char *stype, const char *vartype, const char *name)
{
	
	const int SDNAnr = DNA_struct_find_nr(sdna, stype);
	
	if (SDNAnr != -1) {
		const short * const spo = sdna->structs[SDNAnr];
		const char * const cp = find_elem(sdna, vartype, name, spo, NULL, NULL);
		
		if (cp) {
			return true;
		}
	}
	return false;
}


/**
 * Returns the size in bytes of a primitive type.
 */
int DNA_elem_type_size(const eSDNA_Type elem_nr)
{
	/* should contain all enum types */
	switch (elem_nr) {
		case SDNA_TYPE_CHAR:
		case SDNA_TYPE_UCHAR:
			return 1;
		case SDNA_TYPE_SHORT:
		case SDNA_TYPE_USHORT:
			return 2;
		case SDNA_TYPE_INT:
		case SDNA_TYPE_LONG:
		case SDNA_TYPE_ULONG:
		case SDNA_TYPE_FLOAT:
			return 4;
		case SDNA_TYPE_DOUBLE:
		case SDNA_TYPE_INT64:
		case SDNA_TYPE_UINT64:
			return 8;
	}

	/* weak */
	return 8;
}
