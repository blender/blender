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
 */

/**  \file makesdna.c
 *   \brief Struct muncher for making SDNA.
 *   \ingroup DNA
 *
 * \section aboutmakesdnac About makesdna tool
 * Originally by Ton, some mods by Frank, and some cleaning and
 * extension by Nzc.
 *
 * Makesdna creates a .c file with a long string of numbers that
 * encode the Blender file format. It is fast, because it is basically
 * a binary dump. There are some details to mind when reconstructing
 * the file (endianness and byte-alignment).
 *
 * This little program scans all structs that need to be serialized,
 * and determined the names and types of all members. It calculates
 * how much memory (on disk or in ram) is needed to store that struct,
 * and the offsets for reaching a particular one.
 *
 * There is a facility to get verbose output from sdna. Search for
 * \ref debugSDNA. This int can be set to 0 (no output) to some int. Higher
 * numbers give more output.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"
#include "DNA_sdna_types.h"

#include "BLO_sys_types.h" // for intptr_t support

#define SDNA_MAX_FILENAME_LENGTH 255


/* Included the path relative from /source/blender/ here, so we can move     */
/* headers around with more freedom.                                         */
static const char *includefiles[] = {

	// if you add files here, please add them at the end
	// of makesdna.c (this file) as well

	"DNA_listBase.h",
	"DNA_vec_types.h",
	"DNA_ID.h",
	"DNA_ipo_types.h",
	"DNA_key_types.h",
	"DNA_text_types.h",
	"DNA_packedFile_types.h",
	"DNA_camera_types.h",
	"DNA_image_types.h",
	"DNA_texture_types.h",
	"DNA_lamp_types.h",
	"DNA_material_types.h",
	"DNA_vfont_types.h",
	// if you add files here, please add them at the end
	// of makesdna.c (this file) as well
	"DNA_meta_types.h",
	"DNA_curve_types.h",
	"DNA_mesh_types.h",
	"DNA_meshdata_types.h",
	"DNA_modifier_types.h",
	"DNA_lattice_types.h",	
	"DNA_object_types.h",
	"DNA_object_force.h",
	"DNA_object_fluidsim.h",
	"DNA_world_types.h",
	"DNA_scene_types.h",
	"DNA_view3d_types.h",
	"DNA_view2d_types.h",	
	"DNA_space_types.h",
	"DNA_userdef_types.h",
	"DNA_screen_types.h",
	"DNA_sdna_types.h",
	// if you add files here, please add them at the end
	// of makesdna.c (this file) as well
	"DNA_fileglobal_types.h",
	"DNA_sequence_types.h",
	"DNA_effect_types.h",
	"DNA_outliner_types.h",
	"DNA_property_types.h",
	"DNA_sensor_types.h",
	"DNA_controller_types.h",
	"DNA_actuator_types.h",
	"DNA_sound_types.h",
	"DNA_group_types.h",
	"DNA_armature_types.h",
	"DNA_action_types.h",
	"DNA_constraint_types.h",
	"DNA_nla_types.h",
	"DNA_node_types.h",
	"DNA_color_types.h",
	"DNA_brush_types.h",
	"DNA_customdata_types.h",
	"DNA_particle_types.h",
	"DNA_cloth_types.h",
	"DNA_gpencil_types.h",
	// if you add files here, please add them at the end
	// of makesdna.c (this file) as well
	"DNA_windowmanager_types.h",
	"DNA_anim_types.h",
	"DNA_boid_types.h",
	"DNA_smoke_types.h",
	"DNA_speaker_types.h",
	"DNA_movieclip_types.h",
	"DNA_tracking_types.h",
	"DNA_dynamicpaint_types.h",

	// empty string to indicate end of includefiles
	""
};

static int maxdata= 500000, maxnr= 50000;
static int nr_names=0;
static int nr_types=0;
static int nr_structs=0;
static char **names, *namedata;		/* at address names[a] is string a */
static char **types, *typedata;		/* at address types[a] is string a */
static short *typelens;				/* at typelens[a] is de length of type a */
static short *alphalens;			/* contains sizes as they are calculated on the DEC Alpha (64 bits), infact any 64bit system */
static short **structs, *structdata;/* at sp= structs[a] is the first address of a struct definition
                                     * sp[0] is type number
                                     * sp[1] is amount of elements
                                     * sp[2] sp[3] is typenr,  namenr (etc) */
/**
 * Variable to control debug output of makesdna.
 * debugSDNA:
 *  - 0 = no output, except errors
 *  - 1 = detail actions
 *  - 2 = full trace, tell which names and types were found
 *  - 4 = full trace, plus all gritty details
 */
static int debugSDNA = 0;
static int additional_slen_offset;

/* ************************************************************************** */
/* Functions                                                                  */
/* ************************************************************************** */

/**
 * Add type \c str to struct indexed by \c len, if it was not yet found.
 * \param str char
 * \param len int
 */
static int add_type(const char *str, int len);

/**
 * Add variable \c str to 
 * \param str
 */
static int add_name(const char *str);

/**
 * Search whether this structure type was already found, and if not,
 * add it.
 */
static short *add_struct(int namecode);

/**
 * Remove comments from this buffer. Assumes that the buffer refers to
 * ascii-code text.
 */
static int preprocess_include(char *maindata, int len);

/**
 * Scan this file for serializable types.
 */ 
static int convert_include(char *filename);

/**
 * Determine how many bytes are needed for an array.
 */ 
static int arraysize(char *astr, int len);

/**
 * Determine how many bytes are needed for each struct.
 */ 
static int calculate_structlens(int);

/**
 * Construct the DNA.c file
 */ 
void dna_write(FILE *file, void *pntr, int size);

/**
 * Report all structures found so far, and print their lengths.
 */
void printStructLenghts(void);



/* ************************************************************************** */
/* Implementation                                                             */
/* ************************************************************************** */

/* ************************* MAKEN DNA ********************** */

static int add_type(const char *str, int len)
{
	int nr;
	char *cp;
	
	/* first do validity check */
	if (str[0]==0) {
		return -1;
	}
	else if (strchr(str, '*')) {
		/* note: this is valid C syntax but we can't parse, complain!
		 * 'struct SomeStruct* somevar;' <-- correct but we cant handle right now. */
		return -1;
	}
	
	/* search through type array */
	for (nr=0; nr<nr_types; nr++) {
		if (strcmp(str, types[nr])==0) {
			if (len) {
				typelens[nr]= len;
				alphalens[nr] = len;
			}
			return nr;
		}
	}
	
	/* append new type */
	if (nr_types==0) cp= typedata;
	else {
		cp= types[nr_types-1]+strlen(types[nr_types-1])+1;
	}
	strcpy(cp, str);
	types[nr_types]= cp;
	typelens[nr_types]= len;
	alphalens[nr_types]= len;
	
	if (nr_types>=maxnr) {
		printf("too many types\n");
		return nr_types-1;
	}
	nr_types++;
	
	return nr_types-1;
}


/**
 *
 * Because of the weird way of tokenizing, we have to 'cast' function
 * pointers to ... (*f)(), whatever the original signature. In fact,
 * we add name and type at the same time... There are two special
 * cases, unfortunately. These are explicitly checked.
 *
 * */
static int add_name(const char *str)
{
	int nr, i, j, k;
	char *cp;
	char buf[255]; /* stupid limit, change it :) */
	const char *name;

	additional_slen_offset = 0;
	
	if (str[0]==0 /*  || (str[1]==0) */) return -1;

	if (str[0] == '(' && str[1] == '*') {
		/* we handle function pointer and special array cases here, e.g.
		 * void (*function)(...) and float (*array)[..]. the array case
		 * name is still converted to (array*)() though because it is that
		 * way in old dna too, and works correct with elementsize() */
		int isfuncptr = (strchr(str+1, '(')) != NULL;

		if (debugSDNA > 3) printf("\t\t\t\t*** Function pointer or multidim array pointer found\n");
		/* functionpointer: transform the type (sometimes) */
		i = 0;

		while (str[i] != ')') {
			buf[i] = str[i];
			i++;
		}
		
		/* Another number we need is the extra slen offset. This extra
		 * offset is the overshoot after a space. If there is no
		 * space, no overshoot should be calculated. */
		j = i; /* j at first closing brace */

		if (debugSDNA > 3) printf("first brace after offset %d\n", i);

		j++; /* j beyond closing brace ? */
		while ((str[j] != 0) && (str[j] != ')' )) {
			if (debugSDNA > 3) printf("seen %c ( %d)\n", str[j], str[j]);
			j++;
		}
		if (debugSDNA > 3) printf("seen %c ( %d)\n"
		                          "special after offset%d\n",
		                          str[j], str[j], j);
				
		if (!isfuncptr) {
			/* multidimensional array pointer case */
			if (str[j] == 0) {
				if (debugSDNA > 3) printf("offsetting for multidim array pointer\n");
			}
			else
				printf("Error during tokening multidim array pointer\n");
		}
		else if (str[j] == 0 ) {
			if (debugSDNA > 3) printf("offsetting for space\n"); 
			/* get additional offset */
			k = 0;
			while (str[j] != ')') {
				j++;
				k++;
			}
			if (debugSDNA > 3) printf("extra offset %d\n", k);
			additional_slen_offset = k;
		}
		else if (str[j] == ')' ) {
			if (debugSDNA > 3) printf("offsetting for brace\n");
			; /* don't get extra offset */
		}
		else {
			printf("Error during tokening function pointer argument list\n");
		}
				
		/*
		 * Put )(void) at the end? Maybe )(). Should check this with
		 * old sdna. Actually, sometimes )(), sometimes )(void...)
		 * Alas.. such is the nature of braindamage :(
		 *
		 * Sorted it out: always do )(), except for headdraw and
		 * windraw, part of ScrArea. This is important, because some
		 * linkers will treat different fp's differently when called
		 * !!! This has to do with interference in byte-alignment and
		 * the way args are pushed on the stack.
		 *
		 * */
		buf[i] = 0;
		if (debugSDNA > 3) printf("Name before chomping: %s\n", buf); 
		if ((strncmp(buf, "(*headdraw", 10) == 0) ||
		    (strncmp(buf, "(*windraw", 9) == 0) )
		{
			buf[i] = ')';
			buf[i+1] = '(';
			buf[i+2] = 'v'; 
			buf[i+3] = 'o';
			buf[i+4] = 'i';
			buf[i+5] = 'd';
			buf[i+6] = ')';
			buf[i+7] = 0;
		}
		else {
			buf[i] = ')';
			buf[i+1] = '(';
			buf[i+2] = ')';
			buf[i+3] = 0;
		}
		/* now precede with buf*/
		if (debugSDNA > 3)  printf("\t\t\t\t\tProposing fp name %s\n", buf);
		name = buf;
	}
	else {
		/* normal field: old code */
		name = str;
	}
	
	/* search name array */
	for (nr=0; nr<nr_names; nr++) {
		if (strcmp(name, names[nr])==0) {
			return nr;
		}
	}
	
	/* append new type */
	if (nr_names==0) cp= namedata;
	else {
		cp= names[nr_names-1]+strlen(names[nr_names-1])+1;
	}
	strcpy(cp, name);
	names[nr_names]= cp;
	
	if (nr_names>=maxnr) {
		printf("too many names\n");
		return nr_names-1;
	}
	nr_names++;
	
	return nr_names-1;
}

static short *add_struct(int namecode)
{
	int len;
	short *sp;

	if (nr_structs==0) {
		structs[0]= structdata;
	}
	else {
		sp= structs[nr_structs-1];
		len= sp[1];
		structs[nr_structs]= sp+ 2*len+2;
	}
	
	sp= structs[nr_structs];
	sp[0]= namecode;
	
	if (nr_structs>=maxnr) {
		printf("too many structs\n");
		return sp;
	}
	nr_structs++;
	
	return sp;
}

static int preprocess_include(char *maindata, int len)
{
	int a, newlen, comment = 0;
	char *cp, *temp, *md;
	
	/* note: len + 1, last character is a dummy to prevent
	 * comparisons using uninitialized memory */
	temp= MEM_mallocN(len + 1, "preprocess_include");
	temp[len]= ' ';

	memcpy(temp, maindata, len);
	
	// remove all c++ comments
	/* replace all enters/tabs/etc with spaces */
	cp= temp;
	a= len;
	comment = 0;
	while (a--) {
		if (cp[0]=='/' && cp[1]=='/') {
			comment = 1;
		}
		else if (*cp<32) {
			comment = 0;
		}
		if (comment || *cp<32 || *cp>128 ) *cp= 32;
		cp++;
	}
	

	/* data from temp copy to maindata, remove comments and double spaces */
	cp= temp;
	md= maindata;
	newlen= 0;
	comment= 0;
	a= len;
	while (a--) {
		
		if (cp[0]=='/' && cp[1]=='*') {
			comment= 1;
			cp[0]=cp[1]= 32;
		}
		if (cp[0]=='*' && cp[1]=='/') {
			comment= 0;
			cp[0]=cp[1]= 32;
		}

		/* do not copy when: */
		if (comment);
		else if ( cp[0]==' ' && cp[1]==' ' );
		else if ( cp[-1]=='*' && cp[0]==' ' );	/* pointers with a space */

		/* skip special keywords */
		else if (strncmp("DNA_DEPRECATED", cp, 14)==0) {
			/* single values are skipped already, so decrement 1 less */
			a -= 13;
			cp += 13;
		}
		else {
			md[0]= cp[0];
			md++;
			newlen++;
		}
		cp++;
	}
	
	MEM_freeN(temp);
	return newlen;
}

static void *read_file_data(char *filename, int *len_r)
{
#ifdef WIN32
	FILE *fp= fopen(filename, "rb");
#else
	FILE *fp= fopen(filename, "r");
#endif
	void *data;

	if (!fp) {
		*len_r= -1;
		return NULL;
	}

	fseek(fp, 0L, SEEK_END);
	*len_r= ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	data= MEM_mallocN(*len_r, "read_file_data");
	if (!data) {
		*len_r= -1;
		fclose(fp);
		return NULL;
	}

	if (fread(data, *len_r, 1, fp)!=1) {
		*len_r= -1;
		MEM_freeN(data);
		fclose(fp);
		return NULL;
	}
	
	fclose(fp);
	return data;
}

static int convert_include(char *filename)
{
	/* read include file, skip structs with a '#' before it.
	 * store all data in temporal arrays.
	 */
	int filelen, count, overslaan, slen, type, name, strct;
	short *structpoin, *sp;
	char *maindata, *mainend, *md, *md1;
	
	md= maindata= read_file_data(filename, &filelen);
	if (filelen==-1) {
		printf("Can't read file %s\n", filename);
		return 1;
	}

	filelen= preprocess_include(maindata, filelen);
	mainend= maindata+filelen-1;

	/* we look for '{' and then back to 'struct' */
	count= 0;
	overslaan= 0;
	while (count<filelen) {
		
		/* code for skipping a struct: two hashes on 2 lines. (preprocess added a space) */
		if (md[0]=='#' && md[1]==' ' && md[2]=='#') {
			overslaan= 1;
		}
		
		if (md[0]=='{') {
			md[0]= 0;
			if (overslaan) {
				overslaan= 0;
			}
			else {
				if (md[-1]==' ') md[-1]= 0;
				md1= md-2;
				while ( *md1!=32) md1--;		/* to beginning of word */
				md1++;
				
				/* we've got a struct name when... */
				if ( strncmp(md1-7, "struct", 6)==0 ) {

					strct = add_type(md1, 0);
					if (strct == -1) {
						printf("File '%s' contains struct we cant parse \"%s\"\n", filename, md1);
						return 1;
					}

					structpoin= add_struct(strct);
					sp= structpoin+2;

					if (debugSDNA > 1) printf("\t|\t|-- detected struct %s\n", types[strct]);

					/* first lets make it all nice strings */
					md1= md+1;
					while (*md1 != '}') {
						if (md1>mainend) break;
						
						if (*md1==',' || *md1==' ') *md1= 0;
						md1++;
					}
					
					/* read types and names until first character that is not '}' */
					md1= md+1;
					while ( *md1 != '}' ) {
						if (md1>mainend) break;
						
						/* skip when it says 'struct' or 'unsigned' or 'const' */
						if (*md1) {
							if ( strncmp(md1, "struct", 6)==0 ) md1+= 7;
							if ( strncmp(md1, "unsigned", 8)==0 ) md1+= 9;
							if ( strncmp(md1, "const", 5)==0 ) md1+= 6;
							
							/* we've got a type! */
							type= add_type(md1, 0);
							if (type == -1) {
								printf("File '%s' contains struct we can't parse \"%s\"\n", filename, md1);
								return 1;
							}

							if (debugSDNA > 1) printf("\t|\t|\tfound type %s (", md1);

							md1+= strlen(md1);

							
							/* read until ';' */
							while ( *md1 != ';' ) {
								if (md1>mainend) break;
								
								if (*md1) {
									/* We've got a name. slen needs
									 * correction for function
									 * pointers! */
									slen= (int) strlen(md1);
									if ( md1[slen-1]==';' ) {
										md1[slen-1]= 0;


										name= add_name(md1);
										slen += additional_slen_offset;
										sp[0]= type;
										sp[1]= name;

										if ((debugSDNA>1) && (names[name] != NULL)) printf("%s |", names[name]);

										structpoin[1]++;
										sp+= 2;
																					
										md1+= slen;
										break;
									}
									

									name= add_name(md1);
									slen += additional_slen_offset;

									sp[0]= type;
									sp[1]= name;
									if ((debugSDNA > 1) && (names[name] != NULL)) printf("%s ||", names[name]);

									structpoin[1]++;
									sp+= 2;
									
									md1+= slen;
								}
								md1++;
							}

							if (debugSDNA > 1) printf(")\n");

						}
						md1++;
					}
				}
			}
		}
		count++;
		md++;
	}
	
	MEM_freeN(maindata);

	return 0;
}

static int arraysize(char *astr, int len)
{
	int a, mul=1;
	char str[100], *cp=NULL;

	memcpy(str, astr, len+1);
	
	for (a=0; a<len; a++) {
		if ( str[a]== '[' ) {
			cp= &(str[a+1]);
		}
		else if ( str[a]==']' && cp) {
			str[a]= 0;
			/* if 'cp' is a preprocessor definition, it will evaluate to 0,
			 * the caller needs to check for this case and throw an error */
			mul*= atoi(cp);
		}
	}
	
	return mul;
}

static int calculate_structlens(int firststruct)
{
	int a, b, len, alphalen, unknown= nr_structs, lastunknown, structtype, type, mul, namelen;
	short *sp, *structpoin;
	char *cp;
	int has_pointer, dna_error = 0;
		
	while (unknown) {
		lastunknown= unknown;
		unknown= 0;
		
		/* check all structs... */
		for (a=0; a<nr_structs; a++) {
			structpoin= structs[a];
			structtype= structpoin[0];

			/* when length is not known... */
			if (typelens[structtype]==0) {
				
				sp= structpoin+2;
				len= 0;
				alphalen = 0;
				has_pointer = 0;
				
				/* check all elements in struct */
				for (b=0; b<structpoin[1]; b++, sp+=2) {
					type= sp[0];
					cp= names[sp[1]];

					namelen= (int) strlen(cp);
					/* is it a pointer or function pointer? */
					if (cp[0]=='*' || cp[1]=='*') {
						has_pointer = 1;
						/* has the name an extra length? (array) */
						mul= 1;
						if ( cp[namelen-1]==']') mul= arraysize(cp, namelen);

						if (mul == 0) {
							printf("Zero array size found or could not parse %s: '%.*s'\n", types[structtype], namelen + 1, cp);
							dna_error = 1;
						}

						/* 4-8 aligned/ */
						if (sizeof(void *) == 4) {
							if (len % 4) {
								printf("Align pointer error in struct (len4): %s %s\n", types[structtype], cp);
								dna_error = 1;
							}
						}
						else {
							if (len % 8) {
								printf("Align pointer error in struct (len8): %s %s\n", types[structtype], cp);
								dna_error = 1;
							}
						}

						if (alphalen % 8) {
							printf("Align pointer error in struct (alphalen8): %s %s\n", types[structtype], cp);
							dna_error = 1;
						}

						len += sizeof(void *) * mul;
						alphalen += 8 * mul;

					}
					else if (cp[0]=='[') {
						/* parsing can cause names "var" and "[3]" to be found for "float var [3]" ... */
						printf("Parse error in struct, invalid member name: %s %s\n", types[structtype], cp);
						dna_error = 1;
					}
					else if ( typelens[type] ) {
						/* has the name an extra length? (array) */
						mul= 1;
						if ( cp[namelen-1]==']') mul= arraysize(cp, namelen);

						if (mul == 0) {
							printf("Zero array size found or could not parse %s: '%.*s'\n", types[structtype], namelen + 1, cp);
							dna_error = 1;
						}

						/* struct alignment */
						if (type >= firststruct) {
							if (sizeof(void *)==8 && (len % 8) ) {
								printf("Align struct error: %s %s\n", types[structtype], cp);
								dna_error = 1;
							}
						}
						
						/* 2-4 aligned/ */
						if (typelens[type]>3 && (len % 4) ) {
							printf("Align 4 error in struct: %s %s (add %d padding bytes)\n", types[structtype], cp, len%4);
							dna_error = 1;
						}
						else if (typelens[type]==2 && (len % 2) ) {
							printf("Align 2 error in struct: %s %s (add %d padding bytes)\n", types[structtype], cp, len%2);
							dna_error = 1;
						}

						len += mul*typelens[type];
						alphalen += mul * alphalens[type];
						
					}
					else {
						len= 0;
						alphalen = 0;
						break;
					}
				}
				
				if (len==0) {
					unknown++;
				}
				else {
					typelens[structtype]= len;
					alphalens[structtype]= alphalen;
					// two ways to detect if a struct contains a pointer:
					// has_pointer is set or alphalen != len
					if (has_pointer || alphalen != len) {
						if (alphalen % 8) {
							printf("Sizeerror 8 in struct: %s (add %d bytes)\n", types[structtype], alphalen%8);
							dna_error = 1;
						}
					}
					
					if (len % 4) {
						printf("Sizeerror 4 in struct: %s (add %d bytes)\n", types[structtype], len%4);
						dna_error = 1;
					}
					
				}
			}
		}
		
		if (unknown==lastunknown) break;
	}
	
	if (unknown) {
		printf("ERROR: still %d structs unknown\n", unknown);

		if (debugSDNA) {
			printf("*** Known structs :\n");
			
			for (a=0; a<nr_structs; a++) {
				structpoin= structs[a];
				structtype= structpoin[0];
				
				/* length unknown */
				if (typelens[structtype]!=0) {
					printf("  %s\n", types[structtype]);
				}
			}
		}

			
		printf("*** Unknown structs :\n");
			
		for (a=0; a<nr_structs; a++) {
			structpoin= structs[a];
			structtype= structpoin[0];

			/* length unknown yet */
			if (typelens[structtype]==0) {
				printf("  %s\n", types[structtype]);
			}
		}

		dna_error = 1;
	}

	return(dna_error);
}

#define MAX_DNA_LINE_LENGTH 20

void dna_write(FILE *file, void *pntr, int size)
{
	static int linelength = 0;
	int i;
	char *data;

	data = (char *) pntr;
	
	for (i = 0 ; i < size ; i++) {
		fprintf(file, "%d, ", data[i]);
		linelength++;
		if (linelength >= MAX_DNA_LINE_LENGTH) {
			fprintf(file, "\n");
			linelength = 0;
		}
	}
}

void printStructLenghts(void)
{
	int a, unknown= nr_structs, structtype;
	/*int lastunknown;*/ /*UNUSED*/
	short *structpoin;
	printf("\n\n*** All detected structs:\n");

	while (unknown) {
		/*lastunknown= unknown;*/ /*UNUSED*/
		unknown= 0;
		
		/* check all structs... */
		for (a=0; a<nr_structs; a++) {
			structpoin= structs[a];
			structtype= structpoin[0];
			printf("\t%s\t:%d\n", types[structtype], typelens[structtype]);
		}
	}

	printf("*** End of list\n");

}


static int make_structDNA(char *baseDirectory, FILE *file)
{
	int len, i;
	short *sp;
	/* str contains filenames. Since we now include paths, I stretched       */
	/* it a bit. Hope this is enough :) -nzc-                                */
	char str[SDNA_MAX_FILENAME_LENGTH], *cp;
	int firststruct;
	
	if (debugSDNA > -1) {
		fflush(stdout);
		printf("Running makesdna at debug level %d\n", debugSDNA);
	}
		
	/* the longest known struct is 50k, so we assume 100k is sufficent! */
	namedata= MEM_callocN(maxdata, "namedata");
	typedata= MEM_callocN(maxdata, "typedata");
	structdata= MEM_callocN(maxdata, "structdata");
	
	/* a maximum of 5000 variables, must be sufficient? */
	names= MEM_callocN(sizeof(char *)*maxnr, "names");
	types= MEM_callocN(sizeof(char *)*maxnr, "types");
	typelens= MEM_callocN(sizeof(short)*maxnr, "typelens");
	alphalens= MEM_callocN(sizeof(short)*maxnr, "alphalens");
	structs= MEM_callocN(sizeof(short)*maxnr, "structs");

	/* insertion of all known types */
	/* watch it: uint is not allowed! use in structs an unsigned int */
	/* watch it: sizes must match DNA_elem_type_size() */
	add_type("char", 1);	/* SDNA_TYPE_CHAR */
	add_type("uchar", 1);	/* SDNA_TYPE_UCHAR */
	add_type("short", 2);	/* SDNA_TYPE_SHORT */
	add_type("ushort", 2);	/* SDNA_TYPE_USHORT */
	add_type("int", 4);		/* SDNA_TYPE_INT */
	add_type("long", 4);	/* SDNA_TYPE_LONG */		/* should it be 8 on 64 bits? */
	add_type("ulong", 4);	/* SDNA_TYPE_ULONG */
	add_type("float", 4);	/* SDNA_TYPE_FLOAT */
	add_type("double", 8);	/* SDNA_TYPE_DOUBLE */
	add_type("int64_t", 8);	/* SDNA_TYPE_INT64 */
	add_type("uint64_t", 8); /* SDNA_TYPE_UINT64 */
	add_type("void", 0);	/* SDNA_TYPE_VOID */

	// the defines above shouldn't be output in the padding file...
	firststruct = nr_types;
	
	/* add all include files defined in the global array                     */
	/* Since the internal file+path name buffer has limited length, I do a   */
	/* little test first...                                                  */
	/* Mind the breaking condition here!                                     */
	if (debugSDNA) printf("\tStart of header scan:\n"); 
	for (i = 0; strlen(includefiles[i]); i++) {
		sprintf(str, "%s%s", baseDirectory, includefiles[i]);
		  if (debugSDNA) printf("\t|-- Converting %s\n", str); 
		if (convert_include(str)) {
			return (1);
		}
	}
	if (debugSDNA) printf("\tFinished scanning %d headers.\n", i); 

	if (calculate_structlens(firststruct)) {
		// error
		return(1);
	}

	/* FOR DEBUG */
	if (debugSDNA > 1) {
		int a, b;
/*  		short *elem; */
		short num_types;

		printf("nr_names %d nr_types %d nr_structs %d\n", nr_names, nr_types, nr_structs);
		for (a=0; a<nr_names; a++) { 
			printf(" %s\n", names[a]);
		}
		printf("\n");
		
		sp= typelens;
		for (a=0; a<nr_types; a++, sp++) { 
			printf(" %s %d\n", types[a], *sp);
		}
		printf("\n");
		
		for (a=0; a<nr_structs; a++) {
			sp= structs[a];
			printf(" struct %s elems: %d size: %d\n", types[sp[0]], sp[1], typelens[sp[0]]);
			num_types  = sp[1];
			sp+= 2;
			/* ? num_types was elem? */
			for (b=0; b< num_types; b++, sp+= 2) {
				printf("   %s %s\n", types[sp[0]], names[sp[1]]);
			}
		}
	}

	/* file writing */

	if (debugSDNA > -1) printf("Writing file ... ");
		
	if (nr_names==0 || nr_structs==0);
	else {
		strcpy(str, "SDNA");
		dna_write(file, str, 4);
		
		/* write names */
		strcpy(str, "NAME");
		dna_write(file, str, 4);
		len= nr_names;
		dna_write(file, &len, 4);
		
		/* calculate size of datablock with strings */
		cp= names[nr_names-1];
		cp+= strlen(names[nr_names-1]) + 1;			/* +1: null-terminator */
		len= (intptr_t) (cp - (char*) names[0]);
		len= (len+3) & ~3;
		dna_write(file, names[0], len);
		
		/* write TYPES */
		strcpy(str, "TYPE");
		dna_write(file, str, 4);
		len= nr_types;
		dna_write(file, &len, 4);
	
		/* calculate datablock size */
		cp= types[nr_types-1];
		cp+= strlen(types[nr_types-1]) + 1;		/* +1: null-terminator */
		len= (intptr_t) (cp - (char*) types[0]);
		len= (len+3) & ~3;
		
		dna_write(file, types[0], len);
		
		/* WRITE TYPELENGTHS */
		strcpy(str, "TLEN");
		dna_write(file, str, 4);
		
		len= 2*nr_types;
		if (nr_types & 1) len+= 2;
		dna_write(file, typelens, len);
		
		/* WRITE STRUCTS */
		strcpy(str, "STRC");
		dna_write(file, str, 4);
		len= nr_structs;
		dna_write(file, &len, 4);
	
		/* calc datablock size */
		sp= structs[nr_structs-1];
		sp+= 2+ 2*( sp[1] );
		len= (intptr_t) ((char*) sp - (char*) structs[0]);
		len= (len+3) & ~3;
		
		dna_write(file, structs[0], len);
	
		/* a simple dna padding test */
		if (0) {
			FILE *fp;
			int a;
			
			fp= fopen("padding.c", "w");
			if (fp==NULL);
			else {

				// add all include files defined in the global array
				for (i = 0; strlen(includefiles[i]); i++) {
					fprintf(fp, "#include \"%s%s\"\n", baseDirectory, includefiles[i]);
				}

				fprintf(fp, "main() {\n");
				sp = typelens;
				sp += firststruct;
				for (a=firststruct; a<nr_types; a++, sp++) { 
					if (*sp) {
						fprintf(fp, "\tif (sizeof(struct %s) - %d) printf(\"ALIGN ERROR:", types[a], *sp);
						fprintf(fp, "%%d %s %d ", types[a], *sp);
						fprintf(fp, "\\n\",  sizeof(struct %s) - %d);\n", types[a], *sp);
					}
				}
				fprintf(fp, "}\n");
				fclose(fp);
			}
		}
		/*	end end padding test */
	}
	
	
	MEM_freeN(namedata);
	MEM_freeN(typedata);
	MEM_freeN(structdata);
	MEM_freeN(names);
	MEM_freeN(types);
	MEM_freeN(typelens);
	MEM_freeN(alphalens);
	MEM_freeN(structs);

	if (debugSDNA > -1) printf("done.\n");
	
	return(0);
}

/* ************************* END MAKE DNA ********************** */

static void make_bad_file(const char *file, int line)
{
	FILE *fp= fopen(file, "w");
	fprintf(fp, "#error \"Error! can't make correct DNA.c file from %s:%d, STUPID!\"\n", __FILE__, line);
	fclose(fp);
}

#ifndef BASE_HEADER
#define BASE_HEADER "../"
#endif

int main(int argc, char ** argv)
{
	FILE *file;
	int return_status = 0;

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

			if (argc==3) {
				strcpy(baseDirectory, argv[2]);
			}
			else {
				strcpy(baseDirectory, BASE_HEADER);
			}

			fprintf (file, "unsigned char DNAstr[]= {\n");
			if (make_structDNA(baseDirectory, file)) {
				// error
				fclose(file);
				make_bad_file(argv[1], __LINE__);
				return_status = 1;
			}
			else {
				fprintf(file, "};\n");
				fprintf(file, "int DNAlen= sizeof(DNAstr);\n");
	
				fclose(file);
			}
		}
	}

		
	return(return_status);
}

/* include files for automatic dependencies */
#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "DNA_ID.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_text_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_texture_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_vfont_types.h"
#include "DNA_meta_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_lattice_types.h"	
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_object_fluidsim.h"
#include "DNA_world_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_view2d_types.h"	
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_sequence_types.h"
#include "DNA_effect_types.h"
#include "DNA_outliner_types.h"
#include "DNA_property_types.h"
#include "DNA_sensor_types.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"
#include "DNA_sound_types.h"
#include "DNA_group_types.h"
#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_color_types.h"
#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_particle_types.h"
#include "DNA_cloth_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_anim_types.h"
#include "DNA_boid_types.h"
#include "DNA_smoke_types.h"
#include "DNA_speaker_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"
#include "DNA_dynamicpaint_types.h"
/* end of list */
