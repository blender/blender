/* dna_genfile.c
 *
 * Functions for struct-dna, the genetic file dot c!
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h" // for MEM_freeN MEM_mallocN MEM_callocN

#include "DNA_genfile.h"
#include "DNA_sdna_types.h" // for SDNA ;-)


/* gcc 4.1 on mingw was complaining that __int64 was alredy defined
actually is saw the line below as typedef long long long long... 
Anyhow, since its alredy defined, its safe to do an ifndef here- Cambpell*/
#ifdef FREE_WINDOWS
#ifndef __int64
typedef long long __int64;
#endif
#endif

/*
 * - please note: no builtin security to detect input of double structs
 * - if you want a struct not to be in DNA file: add two hash marks above it (#<enter>#<enter>)

Structure DNA data is added to each blender file and to each executable, this to detect
in .blend files new veriables in structs, changed array sizes, etc. It's also used for
converting endian and pointer size (32-64 bits)
As an extra, Python uses a call to detect run-time the contents of a blender struct.

Create a structDNA: only needed when one of the input include (.h) files change.
File Syntax:
	SDNA (4 bytes) (magic number)
	NAME (4 bytes)
	<nr> (4 bytes) amount of names (int)
	<string> 
	<string>
	...
	...
	TYPE (4 bytes)
	<nr> amount of types (int)
	<string>
	<string>
	...
	...
	TLEN (4 bytes)
	<len> (short) the lengths of types
	<len>
	...
	...
	STRC (4 bytes)
	<nr> amount of structs (int)
	<typenr><nr_of_elems> <typenr><namenr> <typenr><namenr> ...
	
!!Remember to read/write integer and short aligned!!

 While writing a file, the names of a struct is indicated with a type number,
 to be found with: type= findstruct_nr(SDNA *, char *)
 The value of 'type' corresponds with the the index within the structs array

 For the moment: the complete DNA file is included in a .blend file. For
 the future we can think of smarter methods, like only included the used
 structs. Only needed to keep a file short though...

ALLOWED AND TESTED CHANGES IN STRUCTS:
 - type change (a char to float will be divided by 255)
 - location within a struct (everthing can be randomly mixed up)
 - struct within struct (within struct etc), this is recursive
 - adding new elements, will be default initialized zero
 - remving elements
 - change of array sizes
 - change of a pointer type: when the name doesn't change the contents is copied

NOT YET:
 - array (vec[3]) to float struct (vec3f)

DONE:
 - endian compatibility
 - pointer conversion (32-64 bits)

IMPORTANT:
 - do not use #defines in structs for array lenghts, this cannot be read by the dna functions
 - do not use uint, but unsigned int instead, ushort and ulong are allowed
 - only use a long in Blender if you want this to be the size of a pointer. so it is
   32 bits or 64 bits, dependant at the cpu architecture
 - chars are always unsigned
 - aligment of variables has to be done in such a way, that any system does
   not create 'padding' (gaps) in structures. So make sure that:
   - short: 2 aligned
   - int: 4 aligned
   - float: 4 aligned
   - double: 8 aligned
   - long: 8 aligned
   - struct: 8 aligned
 - the sdna functions have several error prints builtin, always check blender running from a console.
 
*/

/* local */
static int le_int(int temp);
static short le_short(short temp);

/* ************************* ENDIAN STUFF ********************** */

static short le_short(short temp)
{
	short new;
	char *rt=(char *)&temp, *rtn=(char *)&new;

	rtn[0]= rt[1];
	rtn[1]= rt[0];

	return new;
}


static int le_int(int temp)
{
	int new;
	char *rt=(char *)&temp, *rtn=(char *)&new;

	rtn[0]= rt[3];
	rtn[1]= rt[2];
	rtn[2]= rt[1];
	rtn[3]= rt[0];

	return new;
}


/* ************************* MAKE DNA ********************** */

/* allowed duplicate code from makesdna.c */
int DNA_elem_array_size(const char *astr, int len)
{
	int a, mul=1;
	char str[100], *cp=0;

	memcpy(str, astr, len+1);

	for(a=0; a<len; a++) {
		if( str[a]== '[' ) {
			cp= &(str[a+1]);
		}
		else if( str[a]==']' && cp) {
			str[a]= 0;
			mul*= atoi(cp);
		}
	}

	return mul;
}

/* ************************* END MAKE DNA ********************** */

/* ************************* DIV ********************** */

void DNA_sdna_free(SDNA *sdna)
{
	MEM_freeN(sdna->data);
	MEM_freeN(sdna->names);
	MEM_freeN(sdna->types);
	MEM_freeN(sdna->structs);
	
	MEM_freeN(sdna);
}

static int ispointer(char *name)
{
	/* check if pointer or function pointer */
	return (name[0]=='*' || (name[0]=='(' && name[1]=='*'));
}

static int elementsize(SDNA *sdna, short type, short name)
/* call with numbers from struct-array */
{
	int mul, namelen, len;
	char *cp;
	
	cp= sdna->names[name];
	len= 0;
	
	namelen= strlen(cp);
	/* is it a pointer or function pointer? */
	if(ispointer(cp)) {
		/* has the naam an extra length? (array) */
		mul= 1;
		if( cp[namelen-1]==']') mul= DNA_elem_array_size(cp, namelen);
		
		len= sdna->pointerlen*mul;
	}
	else if( sdna->typelens[type] ) {
		/* has the naam an extra length? (array) */
		mul= 1;
		if( cp[namelen-1]==']') mul= DNA_elem_array_size(cp, namelen);
		
		len= mul*sdna->typelens[type];
		
	}
	
	return len;
}

#if 0
static void printstruct(SDNA *sdna, short strnr)
{
	/* is for debug */
	int b, nr;
	short *sp;
	
	sp= sdna->structs[strnr];
	
	printf("struct %s\n", sdna->types[ sp[0] ]);
	nr= sp[1];
	sp+= 2;
	
	for(b=0; b< nr; b++, sp+= 2) {
		printf("   %s %s\n", sdna->types[sp[0]], sdna->names[sp[1]]);
	}
}
#endif

static short *findstruct_name(SDNA *sdna, char *str)
{
	int a;
	short *sp=0;


	for(a=0; a<sdna->nr_structs; a++) {

		sp= sdna->structs[a];
		
		if(strcmp( sdna->types[ sp[0] ], str )==0) return sp;
	}
	
	return 0;
}

int DNA_struct_find_nr(SDNA *sdna, const char *str)
{
	short *sp=0;
	int a;

	if(sdna->lastfind<sdna->nr_structs) {
		sp= sdna->structs[sdna->lastfind];
		if(strcmp( sdna->types[ sp[0] ], str )==0) return sdna->lastfind;
	}

	for(a=0; a<sdna->nr_structs; a++) {

		sp= sdna->structs[a];
		
		if(strcmp( sdna->types[ sp[0] ], str )==0) {
			sdna->lastfind= a;
			return a;
		}
	}
	
	return -1;
}

/* ************************* END DIV ********************** */

/* ************************* READ DNA ********************** */

static void init_structDNA(SDNA *sdna, int do_endian_swap)
/* in sdna->data the data, now we convert that to something understandable */
{
	int *data, *verg;
	intptr_t nr;
	short *sp;
	char str[8], *cp;
	
	verg= (int *)str;
	data= (int *)sdna->data;

	strcpy(str, "SDNA");
	if( *data == *verg ) {
	
		data++;
		
		/* load names array */
		strcpy(str, "NAME");
		if( *data == *verg ) {
			data++;
			
			if(do_endian_swap) sdna->nr_names= le_int(*data);
			else sdna->nr_names= *data;
			
			data++;
			sdna->names= MEM_callocN( sizeof(void *)*sdna->nr_names, "sdnanames");
		}
		else {
			printf("NAME error in SDNA file\n");
			return;
		}
		
		nr= 0;
		cp= (char *)data;
		while(nr<sdna->nr_names) {
			sdna->names[nr]= cp;
			while( *cp) cp++;
			cp++;
			nr++;
		}
		nr= (intptr_t)cp;		/* prevent BUS error */
		nr= (nr+3) & ~3;
		cp= (char *)nr;
		
		/* load type names array */
		data= (int *)cp;
		strcpy(str, "TYPE");
		if( *data == *verg ) {
			data++;
			
			if(do_endian_swap) sdna->nr_types= le_int(*data);
			else sdna->nr_types= *data;
			
			data++;
			sdna->types= MEM_callocN( sizeof(void *)*sdna->nr_types, "sdnatypes");
		}
		else {
			printf("TYPE error in SDNA file\n");
			return;
		}
		
		nr= 0;
		cp= (char *)data;
		while(nr<sdna->nr_types) {
			sdna->types[nr]= cp;
			
			/* this is a patch, to change struct names without a confict with SDNA */
			/* be careful to use it, in this case for a system-struct (opengl/X) */
			
			if( *cp == 'b') {
				/* struct Screen was already used by X, 'bScreen' replaces the old IrisGL 'Screen' struct */
				if( strcmp("bScreen", cp)==0 ) sdna->types[nr]= cp+1;
			}
			
			while( *cp) cp++;
			cp++;
			nr++;
		}
		nr= (intptr_t)cp;		/* prevent BUS error */
		nr= (nr+3) & ~3;
		cp= (char *)nr;
		
		/* load typelen array */
		data= (int *)cp;
		strcpy(str, "TLEN");
		if( *data == *verg ) {
			data++;
			sp= (short *)data;
			sdna->typelens= sp;
			
			if(do_endian_swap) {
				short a, *spo= sp;
				
				a= sdna->nr_types;
				while(a--) {
					spo[0]= le_short(spo[0]);
					spo++;
				}
			}
			
			sp+= sdna->nr_types;
		}
		else {
			printf("TLEN error in SDNA file\n");
			return;
		}
		if(sdna->nr_types & 1) sp++;	/* prevent BUS error */

		/* load struct array */
		data= (int *)sp;
		strcpy(str, "STRC");
		if( *data == *verg ) {
			data++;
			
			if(do_endian_swap) sdna->nr_structs= le_int(*data);
			else sdna->nr_structs= *data;
			
			data++;
			sdna->structs= MEM_callocN( sizeof(void *)*sdna->nr_structs, "sdnastrcs");
		}
		else {
			printf("STRC error in SDNA file\n");
			return;
		}
		
		nr= 0;
		sp= (short *)data;
		while(nr<sdna->nr_structs) {
			sdna->structs[nr]= sp;
			
			if(do_endian_swap) {
				short a;
				
				sp[0]= le_short(sp[0]);
				sp[1]= le_short(sp[1]);
				
				a= sp[1];
				sp+= 2;
				while(a--) {
					sp[0]= le_short(sp[0]);
					sp[1]= le_short(sp[1]);
					sp+= 2;
				}
			}
			else {
				sp+= 2*sp[1]+2;
			}
			
			nr++;
		}
		
		/* finally pointerlen: use struct ListBase to test it, never change the size of it! */
		sp= findstruct_name(sdna, "ListBase");
		/* weird; i have no memory of that... I think I used sizeof(void *) before... (ton) */
		
		sdna->pointerlen= sdna->typelens[ sp[0] ]/2;

		if(sp[1]!=2 || (sdna->pointerlen!=4 && sdna->pointerlen!=8)) {
			printf("ListBase struct error! Needs it to calculate pointerize.\n");
			exit(0);
			/* well, at least sizeof(ListBase) is error proof! (ton) */
		}
		
	}
}

SDNA *DNA_sdna_from_data(void *data, int datalen, int do_endian_swap)
{
	SDNA *sdna= MEM_mallocN(sizeof(*sdna), "sdna");
	
	sdna->lastfind= 0;

	sdna->datalen= datalen;
	sdna->data= MEM_mallocN(datalen, "sdna_data");
	memcpy(sdna->data, data, datalen);
	
	init_structDNA(sdna, do_endian_swap);
	
	return sdna;
}

/* ******************** END READ DNA ********************** */

/* ******************* HANDLE DNA ***************** */

static void recurs_test_compflags(SDNA *sdna, char *compflags, int structnr)
{
	int a, b, typenr, elems;
	short *sp;
	char *cp;
	
	/* check all structs, test if it's inside another struct */
	sp= sdna->structs[structnr];
	typenr= sp[0];
	
	for(a=0; a<sdna->nr_structs; a++) {
		if(a!=structnr && compflags[a]==1) {
			sp= sdna->structs[a];
			elems= sp[1];
			sp+= 2;
			for(b=0; b<elems; b++, sp+=2) {
				if(sp[0]==typenr) {
					cp= sdna->names[ sp[1] ];
					if(!ispointer(cp)) {
						compflags[a]= 2;
						recurs_test_compflags(sdna, compflags, a);
					}
				}
			}
		}
	}
	
}

	/* Unsure of exact function - compares the sdna argument to
	 * newsdna and sets up the information necessary to convert
	 * data written with a dna of oldsdna to inmemory data with a
	 * structure defined by the newsdna sdna (I think). -zr
	 */

/* well, the function below is just a lookup table to speed
 * up reading files. doh! -ton
 */


char *DNA_struct_get_compareflags(SDNA *sdna, SDNA *newsdna)
{
	/* flag: 0: doesn't exist anymore (or not yet)
	 *       1: is equal
	 *       2: is different
	 */
	int a, b;
	short *spold, *spcur;
	char *str1, *str2;
	char *compflags;
	
	if(sdna->nr_structs==0) {
		printf("error: file without SDNA\n");
		return NULL;
	}
		
	compflags= MEM_callocN(sdna->nr_structs, "compflags");

	/* we check all structs in 'sdna' and compare them with 
	 * the structs in 'newsdna'
	 */
	
	for(a=0; a<sdna->nr_structs; a++) {
		spold= sdna->structs[a];
		
		/* search for type in cur */
		spcur= findstruct_name(newsdna, sdna->types[spold[0]]);
		
		if(spcur) {
			compflags[a]= 2;
			
			/* compare length and amount of elems */
			if( spcur[1] == spold[1]) {
				 if( newsdna->typelens[spcur[0]] == sdna->typelens[spold[0]] ) {
					 
					 /* same length, same amount of elems, now per type and name */
					 b= spold[1];
					 spold+= 2;
					 spcur+= 2;
					 while(b > 0) {
						 str1= newsdna->types[spcur[0]];
						 str2= sdna->types[spold[0]];
						 if(strcmp(str1, str2)!=0) break;

						 str1= newsdna->names[spcur[1]];
						 str2= sdna->names[spold[1]];
						 if(strcmp(str1, str2)!=0) break;
						 
						 /* same type and same name, now pointersize */
						 if(ispointer(str1)) {
							 if(sdna->pointerlen!=newsdna->pointerlen) break;
						 }
						 
						 b--;
						 spold+= 2;
						 spcur+= 2;
					 }
					 if(b==0) compflags[a]= 1;

				 }
			}
			
		}
	}

	/* first struct in util.h is struct Link, this is skipped in compflags (als # 0).
	 * was a bug, and this way dirty patched! Solve this later....
	 */
	compflags[0]= 1;

	/* Because structs can be inside structs, we recursively
	 * set flags when a struct is altered
	 */
	for(a=0; a<sdna->nr_structs; a++) {
		if(compflags[a]==2) recurs_test_compflags(sdna, compflags, a);
	}
	
/*
	for(a=0; a<sdna->nr_structs; a++) {
		if(compflags[a]==2) {
			spold= sdna->structs[a];
			printf("changed: %s\n", sdna->types[ spold[0] ]);
		}
	}
*/

	return compflags;
}

static void cast_elem(char *ctype, char *otype, char *name, char *curdata, char *olddata)
{
	double val = 0.0;
	int arrlen, curlen=1, oldlen=1, ctypenr, otypenr;
	
	arrlen= DNA_elem_array_size(name, strlen(name));
	
	/* define otypenr */
	if(strcmp(otype, "char")==0 || (strcmp(otype, "const char")==0)) otypenr= 0; 
	else if((strcmp(otype, "uchar")==0) || (strcmp(otype, "unsigned char")==0)) otypenr= 1;
	else if(strcmp(otype, "short")==0) otypenr= 2; 
	else if((strcmp(otype, "ushort")==0)||(strcmp(otype, "unsigned short")==0)) otypenr= 3;
	else if(strcmp(otype, "int")==0) otypenr= 4;
	else if(strcmp(otype, "long")==0) otypenr= 5;
	else if((strcmp(otype, "ulong")==0)||(strcmp(otype, "unsigned long")==0)) otypenr= 6;
	else if(strcmp(otype, "float")==0) otypenr= 7;
	else if(strcmp(otype, "double")==0) otypenr= 8;
	else return;
	
	/* define ctypenr */
	if(strcmp(ctype, "char")==0) ctypenr= 0; 
	else if(strcmp(ctype, "const char")==0) ctypenr= 0; 
	else if((strcmp(ctype, "uchar")==0)||(strcmp(ctype, "unsigned char")==0)) ctypenr= 1;
	else if(strcmp(ctype, "short")==0) ctypenr= 2; 
	else if((strcmp(ctype, "ushort")==0)||(strcmp(ctype, "unsigned short")==0)) ctypenr= 3;
	else if(strcmp(ctype, "int")==0) ctypenr= 4;
	else if(strcmp(ctype, "long")==0) ctypenr= 5;
	else if((strcmp(ctype, "ulong")==0)||(strcmp(ctype, "unsigned long")==0)) ctypenr= 6;
	else if(strcmp(ctype, "float")==0) ctypenr= 7;
	else if(strcmp(ctype, "double")==0) ctypenr= 8;
	else return;

	/* define lengths */
	if(otypenr < 2) oldlen= 1;
	else if(otypenr < 4) oldlen= 2;
	else if(otypenr < 8) oldlen= 4;
	else oldlen= 8;

	if(ctypenr < 2) curlen= 1;
	else if(ctypenr < 4) curlen= 2;
	else if(ctypenr < 8) curlen= 4;
	else curlen= 8;
	
	while(arrlen>0) {
		switch(otypenr) {
		case 0:
			val= *olddata; break;
		case 1:
			val= *( (unsigned char *)olddata); break;
		case 2:
			val= *( (short *)olddata); break;
		case 3:
			val= *( (unsigned short *)olddata); break;
		case 4:
			val= *( (int *)olddata); break;
		case 5:
			val= *( (int *)olddata); break;
		case 6:
			val= *( (unsigned int *)olddata); break;
		case 7:
			val= *( (float *)olddata); break;
		case 8:
			val= *( (double *)olddata); break;
		}
		
		switch(ctypenr) {
		case 0:
			*curdata= val; break;
		case 1:
			*( (unsigned char *)curdata)= val; break;
		case 2:
			*( (short *)curdata)= val; break;
		case 3:
			*( (unsigned short *)curdata)= val; break;
		case 4:
			*( (int *)curdata)= val; break;
		case 5:
			*( (int *)curdata)= val; break;
		case 6:
			*( (unsigned int *)curdata)= val; break;
		case 7:
			if(otypenr<2) val/= 255;
			*( (float *)curdata)= val; break;
		case 8:
			if(otypenr<2) val/= 255;
			*( (double *)curdata)= val; break;
		}

		olddata+= oldlen;
		curdata+= curlen;
		arrlen--;
	}
}

static void cast_pointer(int curlen, int oldlen, char *name, char *curdata, char *olddata)
{
#ifdef WIN32
	__int64 lval;
#else
	long long lval;
#endif
	int arrlen;
	
	arrlen= DNA_elem_array_size(name, strlen(name));
	
	while(arrlen>0) {
	
		if(curlen==oldlen) {
			memcpy(curdata, olddata, curlen);
		}
		else if(curlen==4 && oldlen==8) {
#ifdef WIN32			
			lval= *( (__int64 *)olddata );
#else
			lval= *( (long long *)olddata );
#endif
			*((int *)curdata) = lval>>3;		/* is of course gambling! */
		}
		else if(curlen==8 && oldlen==4) {
#ifdef WIN32
			 *( (__int64 *)curdata ) = *((int *)olddata);
#else
			 *( (long long *)curdata ) = *((int *)olddata);
#endif
		}
		else {
			/* for debug */
			printf("errpr: illegal pointersize! \n");
		}
		
		olddata+= oldlen;
		curdata+= curlen;
		arrlen--;

	}
}

static int elem_strcmp(char *name, char *oname)
{
	int a=0;
	
	/* strcmp without array part */
	
	while(1) {
		if(name[a] != oname[a]) return 1;
		if(name[a]=='[') break;
		if(name[a]==0) break;
		a++;
	}
	if(name[a] != oname[a]) return 1;
	return 0;
}

static char *find_elem(SDNA *sdna, char *type, char *name, short *old, char *olddata, short **sppo)
{
	int a, elemcount, len;
	char *otype, *oname;
	
	/* without arraypart, so names can differ: return old namenr and type */
	
	/* in old is the old struct */
	elemcount= old[1];
	old+= 2;
	for(a=0; a<elemcount; a++, old+=2) {
	
		otype= sdna->types[old[0]];
		oname= sdna->names[old[1]];
		
		len= elementsize(sdna, old[0], old[1]);
		
		if( elem_strcmp(name, oname)==0 ) {	/* naam equal */
			if( strcmp(type, otype)==0 ) {	/* type equal */
				if(sppo) *sppo= old;
				return olddata;
			}
			
			return 0;
		}
		
		olddata+= len;
	}
	return 0;
}

static void reconstruct_elem(SDNA *newsdna, SDNA *oldsdna, char *type, char *name, char *curdata, short *old, char *olddata)
{
	/* rules: test for NAME:
			- name equal:
				- cast type
			- name partially equal (array differs)
				- type equal: memcpy
				- types casten
	   (nzc 2-4-2001 I want the 'unsigned' bit to be parsed as well. Where
	   can I force this?)
	*/	
	int a, elemcount, len, array, oldsize, cursize, mul;
	char *otype, *oname, *cp;
	
	/* is 'name' an array? */
	cp= name;
	array= 0;
	while( *cp && *cp!='[') {
		cp++; array++;
	}
	if( *cp!= '[' ) array= 0;
	
	/* in old is the old struct */
	elemcount= old[1];
	old+= 2;
	for(a=0; a<elemcount; a++, old+=2) {
		otype= oldsdna->types[old[0]];
		oname= oldsdna->names[old[1]];
		len= elementsize(oldsdna, old[0], old[1]);
		
		if( strcmp(name, oname)==0 ) {	/* name equal */
			
			if(ispointer(name)) {	/* pointer of functionpointer afhandelen */
				cast_pointer(newsdna->pointerlen, oldsdna->pointerlen, name, curdata, olddata);
			}
			else if( strcmp(type, otype)==0 ) {	/* type equal */
				memcpy(curdata, olddata, len);
			}
			else cast_elem(type, otype, name, curdata, olddata);

			return;
		}
		else if(array) {		/* name is an array */

			if(oname[array]=='[' && strncmp(name, oname, array)==0 ) {			/* basis equal */
				
				cursize= DNA_elem_array_size(name, strlen(name));
				oldsize= DNA_elem_array_size(oname, strlen(oname));

				if(ispointer(name)) {		/* handle pointer or functionpointer */
					if(cursize>oldsize) cast_pointer(newsdna->pointerlen, oldsdna->pointerlen, oname, curdata, olddata);
					else cast_pointer(newsdna->pointerlen, oldsdna->pointerlen, name, curdata, olddata);
				}
				else if(name[0]=='*' || strcmp(type, otype)==0 ) {	/* type equal */
					mul= len/oldsize;
					mul*= (cursize < oldsize)? cursize: oldsize;
					memcpy(curdata, olddata, mul);
				}
				else {
					if(cursize>oldsize) cast_elem(type, otype, oname, curdata, olddata);
					else cast_elem(type, otype, name, curdata, olddata);
				}
				return;
			}
		}
		olddata+= len;
	}
}

static void reconstruct_struct(SDNA *newsdna, SDNA *oldsdna, char *compflags, int oldSDNAnr, char *data, int curSDNAnr, char *cur)
{
	/* Recursive!
	 * Per element from cur_struct, read data from old_struct.
	 * If element is a struct, call recursive.
	 */
	int a, elemcount, elen, eleno, mul, mulo, firststructtypenr;
	short *spo, *spc, *sppo;
	char *name, *nameo, *type, *cpo, *cpc;

	if(oldSDNAnr== -1) return;
	if(curSDNAnr== -1) return;

	if( compflags[oldSDNAnr]==1 ) {		/* if recursive: test for equal */
	
		spo= oldsdna->structs[oldSDNAnr];
		elen= oldsdna->typelens[ spo[0] ];
		memcpy( cur, data, elen);
		
		return;
	}

	firststructtypenr= *(newsdna->structs[0]);

	spo= oldsdna->structs[oldSDNAnr];
	spc= newsdna->structs[curSDNAnr];

	elemcount= spc[1];

	spc+= 2;
	cpc= cur;
	for(a=0; a<elemcount; a++, spc+=2) {
		type= newsdna->types[spc[0]];
		name= newsdna->names[spc[1]];
		
		elen= elementsize(newsdna, spc[0], spc[1]);

		/* test: is type a struct? */
		if(spc[0]>=firststructtypenr  &&  !ispointer(name)) {
		
			/* where does the old struct data start (and is there an old one?) */
			cpo= find_elem(oldsdna, type, name, spo, data, &sppo);
			
			if(cpo) {
				oldSDNAnr= DNA_struct_find_nr(oldsdna, type);
				curSDNAnr= DNA_struct_find_nr(newsdna, type);
				
				/* array! */
				mul= DNA_elem_array_size(name, strlen(name));
				nameo= oldsdna->names[sppo[1]];
				mulo= DNA_elem_array_size(nameo, strlen(nameo));
				
				eleno= elementsize(oldsdna, sppo[0], sppo[1]);
				
				elen/= mul;
				eleno/= mulo;
				
				while(mul--) {
					reconstruct_struct(newsdna, oldsdna, compflags, oldSDNAnr, cpo, curSDNAnr, cpc);
					cpo+= eleno;
					cpc+= elen;
					
					/* new struct array larger than old */
					mulo--;
					if(mulo<=0) break;
				}
			}
			else cpc+= elen;
		}
		else {

			reconstruct_elem(newsdna, oldsdna, type, name, cpc, spo, data);
			cpc+= elen;

		}
	}
}

void DNA_struct_switch_endian(SDNA *oldsdna, int oldSDNAnr, char *data)
{
	/* Recursive!
	 * If element is a struct, call recursive.
	 */
	int a, mul, elemcount, elen, elena, firststructtypenr;
	short *spo, *spc, skip;
	char *name, *type, *cpo, *cur, cval;

	if(oldSDNAnr== -1) return;
	firststructtypenr= *(oldsdna->structs[0]);
	
	spo= spc= oldsdna->structs[oldSDNAnr];

	elemcount= spo[1];

	spc+= 2;
	cur= data;
	
	for(a=0; a<elemcount; a++, spc+=2) {
		type= oldsdna->types[spc[0]];
		name= oldsdna->names[spc[1]];
		
		/* elementsize = including arraysize */
		elen= elementsize(oldsdna, spc[0], spc[1]);

		/* test: is type a struct? */
		if(spc[0]>=firststructtypenr  &&  !ispointer(name)) {
			/* where does the old data start (is there one?) */
			cpo= find_elem(oldsdna, type, name, spo, data, 0);
			if(cpo) {
				oldSDNAnr= DNA_struct_find_nr(oldsdna, type);
				
				mul= DNA_elem_array_size(name, strlen(name));
				elena= elen/mul;

				while(mul--) {
					DNA_struct_switch_endian(oldsdna, oldSDNAnr, cpo);
					cpo += elena;
				}
			}
		}
		else {
			
			if(ispointer(name)) {
				if(oldsdna->pointerlen==8) {
					
					mul= DNA_elem_array_size(name, strlen(name));
					cpo= cur;
					while(mul--) {
						cval= cpo[0]; cpo[0]= cpo[7]; cpo[7]= cval;
						cval= cpo[1]; cpo[1]= cpo[6]; cpo[6]= cval;
						cval= cpo[2]; cpo[2]= cpo[5]; cpo[5]= cval;
						cval= cpo[3]; cpo[3]= cpo[4]; cpo[4]= cval;
						
						cpo+= 8;
					}
					
				}
			}
			else {
				
				if( spc[0]==2 || spc[0]==3 ) {	/* short-ushort */
					
					/* exception: variable called blocktype/ipowin: derived from ID_  */
					skip= 0;
					if(name[0]=='b' && name[1]=='l') {
						if(strcmp(name, "blocktype")==0) skip= 1;
					}
					else if(name[0]=='i' && name[1]=='p') {
						if(strcmp(name, "ipowin")==0) skip= 1;
					}
					
					if(skip==0) {
						mul= DNA_elem_array_size(name, strlen(name));
						cpo= cur;
						while(mul--) {
							cval= cpo[0];
							cpo[0]= cpo[1];
							cpo[1]= cval;
							cpo+= 2;
						}
					}
				}
				else if(spc[0]>3 && spc[0]<8) { /* int-long-ulong-float */

					mul= DNA_elem_array_size(name, strlen(name));
					cpo= cur;
					while(mul--) {
						cval= cpo[0];
						cpo[0]= cpo[3];
						cpo[3]= cval;
						cval= cpo[1];
						cpo[1]= cpo[2];
						cpo[2]= cval;
						cpo+= 4;
					}
				}
			}
		}
		cur+= elen;
	}
}

void *DNA_struct_reconstruct(SDNA *newsdna, SDNA *oldsdna, char *compflags, int oldSDNAnr, int blocks, void *data)
{
	int a, curSDNAnr, curlen=0, oldlen;
	short *spo, *spc;
	char *cur, *type, *cpc, *cpo;
	
	/* oldSDNAnr == structnr, we're looking for the corresponding 'cur' number */
	spo= oldsdna->structs[oldSDNAnr];
	type= oldsdna->types[ spo[0] ];
	oldlen= oldsdna->typelens[ spo[0] ];
	curSDNAnr= DNA_struct_find_nr(newsdna, type);

	/* init data and alloc */
	if(curSDNAnr >= 0) {
		spc= newsdna->structs[curSDNAnr];
		curlen= newsdna->typelens[ spc[0] ];
	}
	if(curlen==0) {
		return NULL;
	}

	cur= MEM_callocN( blocks*curlen, "reconstruct");
	cpc= cur;
	cpo= data;
	for(a=0; a<blocks; a++) {
		reconstruct_struct(newsdna, oldsdna, compflags, oldSDNAnr, cpo, curSDNAnr, cpc);
		cpc+= curlen;
		cpo+= oldlen;
	}

	return cur;
}

int DNA_elem_offset(SDNA *sdna, char *stype, char *vartype, char *name)
{
	
	int SDNAnr= DNA_struct_find_nr(sdna, stype);
	short *spo= sdna->structs[SDNAnr];
	char *cp= find_elem(sdna, vartype, name, spo, NULL, NULL);
	return (int)((intptr_t)cp);
}

