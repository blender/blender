/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * DNA handling
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#include <unistd.h> // for read close
#else
#include <io.h> // for open close read
#endif

#include <string.h> // strncmp
#include <stdio.h> // for printf
#include <stdlib.h> // for atoi
#include <fcntl.h> // for open O_RDONLY

#include "MEM_guardedalloc.h" // for MEM_freeN MEM_mallocN MEM_callocN
#include "BLI_blenlib.h"      // for  BLI_filesize

#include "BKE_utildefines.h" // for O_BINARY TRUE MIN2

#include "DNA_sdna_types.h" // for SDNA ;-)

#include "BLO_writefile.h"
#include "BLO_genfile.h"

#include "genfile.h"


/*
 * - pas op: geen beveiling tegen dubbele structen
 * - struct niet in DNA file: twee hekjes erboven (#<enter>#<enter>)
Aanmaken structDNA: alleen wanneer includes wijzigen.
File Syntax:
	SDNA (4 bytes) (voor fileherkenning)
	NAME (4 bytes)
	<nr> (4 bytes) aantal namen (int)
	<string> 
	<string>
	...
	...
	TYPE (4 bytes)
	<nr> aantal types (int)
	<string>
	<string>
	...
	...
	TLEN (4 bytes)
	<len> (short)
	<len>
	...
	...
	STRC (4 bytes)
	<nr> aantal structen (int)
	<typenr><nr_of_elems> <typenr><namenr> <typenr><namenr> ...
	
!!Denk aan integer en short aligned schrijven/lezen!!
 
Bij het wegschrijven van files worden namen structen aangegeven
met type= findstruct_nr(SDNA *, char *), 'type' correspondeert met nummer
structarray in structDNA.

Voor het moment: complete DNA file appenden achter blenderfile.
In toekomst nadenken over slimmere methode (alleen gebruikte
structen?, voorgeprepareerde DNA files? (TOT, OB, MAT )

TOEGESTANE EN GETESTE WIJZIGINGEN IN STRUCTS:
	- type verandering (bij chars naar float wordt door 255 gedeeld)
	- plek in struct (alles kan door elkaar)
	- struct in struct (in struct etc, is recursief)
	- nieuwe elementen toevoegen (standaard op 0)
	- elementen eruit (worden niet meer ingelezen)
	- array's groter/kleiner
	- verschillende typepointers met zelfde naam worden altijd gekopieerd.
(NOG) NIET:
	- float-array (vec[3]) naar struct van floats (vec3f)
GEDAAN:
	- DNA file in (achter) blender-executable plakken voor upward
	compatibility Gebruikte methode: het makesdna programma schrijft
	een c-file met een met spaties gevuld char-array van de juiste
	lengte. Makesdna maakt er een .o van en vult de spaties met de
	DNA file.
	- endian compatibility
	- 32 bits en 64 bits pointers
LET OP:
	- uint mag niet in een struct,  gebruik unsigned int. (backwards
	compatibility vanwege 64 bits code!)
	- structen moeten altijd (intern) 4/8-aligned en short-aligned zijn.
	  de SDNA routine test hierop en print duidelijke errors.
	  DNA files met align errors zijn onbruikbaar!
	- switch_endian doet alleen long long pointers, 
	  zodat ze veilig gecast kunnen worden naar 32 bits
	- casten van 64 naar 32 bits poinetrs: >>3.
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


/* ************************* MAKEN DNA ********************** */

/* allowed duplicate code from makesdna.c */
static int arraysize(char *astr, int len)
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

/* ************************* END MAKEN DNA ********************** */

/* ************************* DIV ********************** */

void dna_freestructDNA(struct SDNA *sdna)
{
	MEM_freeN(sdna->data);
	MEM_freeN(sdna->names);
	MEM_freeN(sdna->types);
	MEM_freeN(sdna->structs);
	
	MEM_freeN(sdna);
}

static int elementsize(struct SDNA *sdna, short type, short name)
/* aanroepen met nummers uit struct-array */
{
	int mul, namelen, len;
	char *cp;
	
	cp= sdna->names[name];
	len= 0;
	
	namelen= strlen(cp);
	/* is het een pointer of functiepointer? */
	if(cp[0]=='*' || cp[1]=='*') {
		/* heeft de naam een extra lente? (array) */
		mul= 1;
		if( cp[namelen-1]==']') mul= arraysize(cp, namelen);
		
		len= sdna->pointerlen*mul;
	}
	else if( sdna->typelens[type] ) {
		/* heeft de naam een extra lente? (array) */
		mul= 1;
		if( cp[namelen-1]==']') mul= arraysize(cp, namelen);
		
		len= mul*sdna->typelens[type];
		
	}
	
	return len;
}

#if 0
static void printstruct(struct SDNA *sdna, short strnr)
{
	/* geef het structnummer door, is voor debug */
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

static short *findstruct_name(struct SDNA *sdna, char *str)
{
	int a;
	short *sp=0;


	for(a=0; a<sdna->nr_structs; a++) {

		sp= sdna->structs[a];
		
		if(strcmp( sdna->types[ sp[0] ], str )==0) return sp;
	}
	
	return 0;
}

int dna_findstruct_nr(struct SDNA *sdna, char *str)
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

/* ************************* LEZEN DNA ********************** */

static void init_structDNA(struct SDNA *sdna, int do_endian_swap)
/* in sdna->data staat de data, uit elkaar pulken */
{
	int *data, *verg;
	long nr;
	short *sp;
	char str[8], *cp;
	
	verg= (int *)str;
	data= (int *)sdna->data;

	strcpy(str, "SDNA");
	if( *data == *verg ) {
	
		data++;
		
		/* laad namen array */
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
		nr= (long)cp;		/* BUS error voorkomen */
		nr= (nr+3) & ~3;
		cp= (char *)nr;
		
		/* laad typenamen array */
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
			
			/* met deze patch kunnen structnamen gewijzigd worden */
			/* alleen gebruiken voor conflicten met systeem-structen (opengl/X) */
			
			if( *cp == 'b') {
				/* struct Screen was already used by X, 'bScreen' replaces the old IrisGL 'Screen' struct */
				if( strcmp("bScreen", cp)==0 ) sdna->types[nr]= cp+1;
			}
			
			while( *cp) cp++;
			cp++;
			nr++;
		}
		nr= (long)cp;		/* BUS error voorkomen */
		nr= (nr+3) & ~3;
		cp= (char *)nr;
		
		/* laad typelen array */
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
		if(sdna->nr_types & 1) sp++;	/* BUS error voorkomen */

		/* laad structen array */
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
		
		sdna->pointerlen= sdna->typelens[ sp[0] ]/2;

		if(sp[1]!=2 || (sdna->pointerlen!=4 && sdna->pointerlen!=8)) {
			printf("ListBase struct error! Needs it to calculate pointerize.\n");
			exit(0);
		}
		
	}
}

struct SDNA *dna_sdna_from_data(void *data, int datalen, int do_endian_swap)
{
	struct SDNA *sdna= MEM_mallocN(sizeof(*sdna), "sdna");
	
	sdna->lastfind= 0;

	sdna->datalen= datalen;
	sdna->data= MEM_mallocN(datalen, "sdna_data");
	memcpy(sdna->data, data, datalen);
	
	init_structDNA(sdna, do_endian_swap);
	
	return sdna;
}
		/* XXX, this routine was added because at one
		 * point I thought using the dna more would be
		 * nice, but really thats a flawed idea, you
		 * already know about your memory structures when
		 * you are compiling no need to redirect that
		 * through the DNA, the python stuff should be
		 * changed to not use this routine (a bit 
		 * o' work). - zr
		 */
int BLO_findstruct_offset(char *structname, char *member)
{
	extern char DNAstr[];	/* DNA.c */
	extern int DNAlen;
	
	struct SDNA *sdna;
	int a, offset;
	short *sp;

	sdna= dna_sdna_from_data(DNAstr, DNAlen, 0);
	
	sp= findstruct_name(sdna, structname);
	
	if(sp) {
		a= sp[1];	/* aantal elems */
		sp+= 2;
		offset= 0;
		
		while(a--) {
			if(strcmp(sdna->names[sp[1]], member)==0) {
				dna_freestructDNA(sdna);
				return offset;
			}

			offset+= elementsize(sdna, sp[0], sp[1]);			
			sp+= 2;
		}
	}
	
	dna_freestructDNA(sdna);
	return -1;
}

/* ******************** END LEZEN DNA ********************** */

/* ******************* AFHANDELEN DNA ***************** */

static void recurs_test_compflags(struct SDNA *sdna, char *compflags, int structnr)
{
	int a, b, typenr, elems;
	short *sp;
	char *cp;
	
	/* loop alle structen af en test of deze struct in andere zit */
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
					if(cp[0]!= '*') {
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
char *dna_get_structDNA_compareflags(struct SDNA *sdna, struct SDNA *newsdna)
{
	/* flag: 0:bestaat niet meer (of nog niet)
	 *       1: is gelijk
	 *       2: is anders
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

	/* We lopen alle structs in 'sdna' af, vergelijken ze met 
	 * de structs in 'newsdna'
	 */
	
	for(a=0; a<sdna->nr_structs; a++) {
		spold= sdna->structs[a];
		
		/* type zoeken in cur */
		spcur= findstruct_name(newsdna, sdna->types[spold[0]]);
		
		if(spcur) {
			compflags[a]= 2;
			
			/* lengte en aantal elems vergelijken */
			if( spcur[1] == spold[1]) {
				 if( newsdna->typelens[spcur[0]] == sdna->typelens[spold[0]] ) {
					 
					 /* evenlang en evenveel elems, nu per type en naam */
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
						 
						 /* naam gelijk, type gelijk, nu nog pointersize, dit geval komt haast nooit voor! */
						 if(str1[0]=='*') {
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

	/* eerste struct in util.h is struct Link, deze wordt in de compflags overgeslagen (als # 0).
	 * Vuile patch! Nog oplossen....
	 */
	compflags[0]= 1;

	/* Aangezien structen in structen kunnen zitten gaan we recursief
	 * vlaggen zetten als er een struct veranderd is
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
	
	arrlen= arraysize(name, strlen(name));
	
	/* otypenr bepalen */
	if(strcmp(otype, "char")==0) otypenr= 0; 
	else if((strcmp(otype, "uchar")==0)||(strcmp(otype, "unsigned char")==0)) otypenr= 1;
	else if(strcmp(otype, "short")==0) otypenr= 2; 
	else if((strcmp(otype, "ushort")==0)||(strcmp(otype, "unsigned short")==0)) otypenr= 3;
	else if(strcmp(otype, "int")==0) otypenr= 4;
	else if(strcmp(otype, "long")==0) otypenr= 5;
	else if((strcmp(otype, "ulong")==0)||(strcmp(otype, "unsigned long")==0)) otypenr= 6;
	else if(strcmp(otype, "float")==0) otypenr= 7;
	else if(strcmp(otype, "double")==0) otypenr= 8;
	else return;
	
	/* ctypenr bepalen */
	if(strcmp(ctype, "char")==0) ctypenr= 0; 
	else if((strcmp(ctype, "uchar")==0)||(strcmp(ctype, "unsigned char")==0)) ctypenr= 1;
	else if(strcmp(ctype, "short")==0) ctypenr= 2; 
	else if((strcmp(ctype, "ushort")==0)||(strcmp(ctype, "unsigned short")==0)) ctypenr= 3;
	else if(strcmp(ctype, "int")==0) ctypenr= 4;
	else if(strcmp(ctype, "long")==0) ctypenr= 5;
	else if((strcmp(ctype, "ulong")==0)||(strcmp(ctype, "unsigned long")==0)) ctypenr= 6;
	else if(strcmp(ctype, "float")==0) ctypenr= 7;
	else if(strcmp(ctype, "double")==0) ctypenr= 8;
	else return;

	/* lengtes bepalen */
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
	
	arrlen= arraysize(name, strlen(name));
	
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
			*((int *)curdata) = lval>>3;		/* is natuurlijk een beetje een gok! */
		}
		else if(curlen==8 && oldlen==4) {
#ifdef WIN32
			 *( (__int64 *)curdata ) = *((int *)olddata);
#else
			 *( (long long *)curdata ) = *((int *)olddata);
#endif
		}
		else {
			/* voor debug */
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
	
	while(TRUE) {
		if(name[a] != oname[a]) return 1;
		if(name[a]=='[') break;
		if(name[a]==0) break;
		a++;
	}
	if(name[a] != oname[a]) return 1;
	return 0;
}

static char *find_elem(struct SDNA *sdna, char *type, char *name, short *old, char *olddata, short **sppo)
{
	int a, elemcount, len;
	char *otype, *oname;
	
	/* without arraypart, so names can differ: return old namenr and type */
	
	/* in old staat de oude struct */
	elemcount= old[1];
	old+= 2;
	for(a=0; a<elemcount; a++, old+=2) {
	
		otype= sdna->types[old[0]];
		oname= sdna->names[old[1]];
		
		len= elementsize(sdna, old[0], old[1]);
		
		if( elem_strcmp(name, oname)==0 ) {	/* naam gelijk */
			if( strcmp(type, otype)==0 ) {	/* type gelijk */
				if(sppo) *sppo= old;
				return olddata;
			}
			
			return 0;
		}
		
		olddata+= len;
	}
	return 0;
}

static void reconstruct_elem(struct SDNA *newsdna, struct SDNA *oldsdna, char *type, char *name, char *curdata, short *old, char *olddata)
{
	/* regels: testen op NAAM:
			- naam volledig gelijk:
				- type casten
			- naam gedeeltelijk gelijk (array anders)
				- type gelijk: memcpy
				- types casten
	   (nzc 2-4-2001 I want the 'unsigned' bit to be parsed as well. Where
	   can I force this?
	*/	
	int a, elemcount, len, array, oldsize, cursize, mul;
	char *otype, *oname, *cp;
	
	/* is 'name' een array? */
	cp= name;
	array= 0;
	while( *cp && *cp!='[') {
		cp++; array++;
	}
	if( *cp!= '[' ) array= 0;
	
	/* in old staat de oude struct */
	elemcount= old[1];
	old+= 2;
	for(a=0; a<elemcount; a++, old+=2) {
		otype= oldsdna->types[old[0]];
		oname= oldsdna->names[old[1]];
		len= elementsize(oldsdna, old[0], old[1]);
		
		if( strcmp(name, oname)==0 ) {	/* naam gelijk */
			
			if( name[0]=='*') {		/* pointer afhandelen */
				cast_pointer(newsdna->pointerlen, oldsdna->pointerlen, name, curdata, olddata);
			}
			else if( strcmp(type, otype)==0 ) {	/* type gelijk */
				memcpy(curdata, olddata, len);
			}
			else cast_elem(type, otype, name, curdata, olddata);

			return;
		}
		else if(array) {		/* de naam is een array */

			if( strncmp(name, oname, array)==0 ) {			/* basis gelijk */
				
				cursize= arraysize(name, strlen(name));
				oldsize= arraysize(oname, strlen(oname));

				if( name[0]=='*') {		/* pointer afhandelen */
					if(cursize>oldsize) cast_pointer(newsdna->pointerlen, oldsdna->pointerlen, oname, curdata, olddata);
					else cast_pointer(newsdna->pointerlen, oldsdna->pointerlen, name, curdata, olddata);
				}
				else if(name[0]=='*' || strcmp(type, otype)==0 ) {	/* type gelijk */
					mul= len/oldsize;
					mul*= MIN2(cursize, oldsize);
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

static void reconstruct_struct(struct SDNA *newsdna, struct SDNA *oldsdna, char *compflags, int oldSDNAnr, char *data, int curSDNAnr, char *cur)
{
	/* Recursief!
	 * Per element van cur_struct data lezen uit old_struct.
	 * Als element een struct is, recursief aanroepen.
	 */
	int a, elemcount, elen, eleno, mul, mulo, firststructtypenr;
	short *spo, *spc, *sppo;
	char *name, *nameo, *type, *cpo, *cpc;

	if(oldSDNAnr== -1) return;
	if(curSDNAnr== -1) return;

	if( compflags[oldSDNAnr]==1 ) {		/* bij recurs: testen op gelijk */
	
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

		/* testen: is type een struct? */
		if(spc[0]>=firststructtypenr  &&  name[0]!='*') {
		
			/* waar start de oude struct data (is ie er wel?) */
			cpo= find_elem(oldsdna, type, name, spo, data, &sppo);
			
			if(cpo) {
				oldSDNAnr= dna_findstruct_nr(oldsdna, type);
				curSDNAnr= dna_findstruct_nr(newsdna, type);
				
				/* array! */
				mul= arraysize(name, strlen(name));
				nameo= oldsdna->names[sppo[1]];
				mulo= arraysize(nameo, strlen(nameo));
				
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

void dna_switch_endian_struct(struct SDNA *oldsdna, int oldSDNAnr, char *data)
{
	/* Recursief!
	 * Als element een struct is, recursief aanroepen.
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

		/* testen: is type een struct? */
		if(spc[0]>=firststructtypenr  &&  name[0]!='*') {
			/* waar start de oude struct data (is ie er wel?) */
			cpo= find_elem(oldsdna, type, name, spo, data, 0);
			if(cpo) {
				oldSDNAnr= dna_findstruct_nr(oldsdna, type);
				
				mul= arraysize(name, strlen(name));
				elena= elen/mul;

				while(mul--) {
					dna_switch_endian_struct(oldsdna, oldSDNAnr, cpo);
					cpo += elena;
				}
			}
		}
		else {
			
			if( name[0]=='*' ) {
				if(oldsdna->pointerlen==8) {
					
					mul= arraysize(name, strlen(name));
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
					
					/* uitzondering: variable die blocktype/ipowin heet: van ID_ afgeleid */
					skip= 0;
					if(name[0]=='b' && name[1]=='l') {
						if(strcmp(name, "blocktype")==0) skip= 1;
					}
					else if(name[0]=='i' && name[1]=='p') {
						if(strcmp(name, "ipowin")==0) skip= 1;
					}
					
					if(skip==0) {
						mul= arraysize(name, strlen(name));
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

					mul= arraysize(name, strlen(name));
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

void *dna_reconstruct(struct SDNA *newsdna, struct SDNA *oldsdna, char *compflags, int oldSDNAnr, int blocks, void *data)
{
	int a, curSDNAnr, curlen=0, oldlen;
	short *spo, *spc;
	char *cur, *type, *cpc, *cpo;
	
	/* oldSDNAnr == structnr, we zoeken het corresponderende 'cur' nummer */
	spo= oldsdna->structs[oldSDNAnr];
	type= oldsdna->types[ spo[0] ];
	oldlen= oldsdna->typelens[ spo[0] ];
	curSDNAnr= dna_findstruct_nr(newsdna, type);

	/* data goedzetten en nieuwe calloc doen */
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

