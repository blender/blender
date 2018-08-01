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

#include "../blenlib/BLI_sys_types.h" // for intptr_t support

#define SDNA_MAX_FILENAME_LENGTH 255


/* Included the path relative from /source/blender/ here, so we can move     */
/* headers around with more freedom.                                         */
static const char *includefiles[] = {

	/* if you add files here, please add them at the end
	 * of makesdna.c (this file) as well */
	"DNA_listBase.h",
	"DNA_vec_types.h",
	"DNA_ID.h",
	"DNA_ipo_types.h",
	"DNA_key_types.h",
	"DNA_text_types.h",
	"DNA_packedFile_types.h",
	"DNA_gpu_types.h",
	"DNA_camera_types.h",
	"DNA_image_types.h",
	"DNA_texture_types.h",
	"DNA_lamp_types.h",
	"DNA_material_types.h",
	"DNA_vfont_types.h",
	"DNA_meta_types.h",
	"DNA_curve_types.h",
	"DNA_mesh_types.h",
	"DNA_meshdata_types.h",
	"DNA_modifier_types.h",
	"DNA_lattice_types.h",
	"DNA_object_types.h",
	"DNA_object_force_types.h",
	"DNA_object_fluidsim_types.h",
	"DNA_world_types.h",
	"DNA_scene_types.h",
	"DNA_view3d_types.h",
	"DNA_view2d_types.h",
	"DNA_space_types.h",
	"DNA_userdef_types.h",
	"DNA_screen_types.h",
	"DNA_sdna_types.h",
	"DNA_fileglobal_types.h",
	"DNA_sequence_types.h",
	"DNA_effect_types.h",
	"DNA_outliner_types.h",
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
	"DNA_gpencil_modifier_types.h",
	"DNA_shader_fx_types.h",
	"DNA_windowmanager_types.h",
	"DNA_anim_types.h",
	"DNA_boid_types.h",
	"DNA_smoke_types.h",
	"DNA_speaker_types.h",
	"DNA_movieclip_types.h",
	"DNA_tracking_types.h",
	"DNA_dynamicpaint_types.h",
	"DNA_mask_types.h",
	"DNA_rigidbody_types.h",
	"DNA_freestyle_types.h",
	"DNA_linestyle_types.h",
	"DNA_cachefile_types.h",
	"DNA_layer_types.h",
	"DNA_workspace_types.h",
	"DNA_lightprobe_types.h",

	/* see comment above before editing! */

	/* empty string to indicate end of includefiles */
	""
};

static int maxdata = 500000, maxnr = 50000;
static int nr_names = 0;
static int nr_types = 0;
static int nr_structs = 0;
static char **names, *namedata;      /* at address names[a] is string a */
static char **types, *typedata;      /* at address types[a] is string a */
static short *typelens_native;       /* at typelens[a] is the length of type 'a' on this systems bitness (32 or 64) */
static short *typelens_32;           /* contains sizes as they are calculated on 32 bit systems */
static short *typelens_64;           /* contains sizes as they are calculated on 64 bit systems */
static short **structs, *structdata; /* at sp = structs[a] is the first address of a struct definition
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
static int convert_include(const char *filename);

/**
 * Determine how many bytes are needed for an array.
 */
static int arraysize(const char *str);

/**
 * Determine how many bytes are needed for each struct.
 */
static int calculate_structlens(int);

/**
 * Construct the DNA.c file
 */
static void dna_write(FILE *file, const void *pntr, const int size);

/**
 * Report all structures found so far, and print their lengths.
 */
void printStructLengths(void);



/* ************************************************************************** */
/* Implementation                                                             */
/* ************************************************************************** */

/* ************************* MAKEN DNA ********************** */

static int add_type(const char *str, int len)
{
	int nr;
	char *cp;

	/* first do validity check */
	if (str[0] == 0) {
		return -1;
	}
	else if (strchr(str, '*')) {
		/* note: this is valid C syntax but we can't parse, complain!
		 * 'struct SomeStruct* somevar;' <-- correct but we cant handle right now. */
		return -1;
	}

	/* search through type array */
	for (nr = 0; nr < nr_types; nr++) {
		if (strcmp(str, types[nr]) == 0) {
			if (len) {
				typelens_native[nr] = len;
				typelens_32[nr] = len;
				typelens_64[nr] = len;
			}
			return nr;
		}
	}

	/* append new type */
	if (nr_types == 0) {
		cp = typedata;
	}
	else {
		cp = types[nr_types - 1] + strlen(types[nr_types - 1]) + 1;
	}
	strcpy(cp, str);
	types[nr_types] = cp;
	typelens_native[nr_types] = len;
	typelens_32[nr_types] = len;
	typelens_64[nr_types] = len;

	if (nr_types >= maxnr) {
		printf("too many types\n");
		return nr_types - 1;
	}
	nr_types++;

	return nr_types - 1;
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

	if (str[0] == 0 /*  || (str[1] == 0) */) return -1;

	if (str[0] == '(' && str[1] == '*') {
		/* we handle function pointer and special array cases here, e.g.
		 * void (*function)(...) and float (*array)[..]. the array case
		 * name is still converted to (array *)() though because it is that
		 * way in old dna too, and works correct with elementsize() */
		int isfuncptr = (strchr(str + 1, '(')) != NULL;

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
		while ((str[j] != 0) && (str[j] != ')')) {
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
		else if (str[j] == 0) {
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
		else if (str[j] == ')') {
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
			buf[i + 1] = '(';
			buf[i + 2] = 'v';
			buf[i + 3] = 'o';
			buf[i + 4] = 'i';
			buf[i + 5] = 'd';
			buf[i + 6] = ')';
			buf[i + 7] = 0;
		}
		else {
			buf[i] = ')';
			buf[i + 1] = '(';
			buf[i + 2] = ')';
			buf[i + 3] = 0;
		}
		/* now precede with buf*/
		if (debugSDNA > 3) printf("\t\t\t\t\tProposing fp name %s\n", buf);
		name = buf;
	}
	else {
		/* normal field: old code */
		name = str;
	}

	/* search name array */
	for (nr = 0; nr < nr_names; nr++) {
		if (strcmp(name, names[nr]) == 0) {
			return nr;
		}
	}

	/* append new type */
	if (nr_names == 0) {
		cp = namedata;
	}
	else {
		cp = names[nr_names - 1] + strlen(names[nr_names - 1]) + 1;
	}
	strcpy(cp, name);
	names[nr_names] = cp;

	if (nr_names >= maxnr) {
		printf("too many names\n");
		return nr_names - 1;
	}
	nr_names++;

	return nr_names - 1;
}

static short *add_struct(int namecode)
{
	int len;
	short *sp;

	if (nr_structs == 0) {
		structs[0] = structdata;
	}
	else {
		sp = structs[nr_structs - 1];
		len = sp[1];
		structs[nr_structs] = sp + 2 * len + 2;
	}

	sp = structs[nr_structs];
	sp[0] = namecode;

	if (nr_structs >= maxnr) {
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
	temp = MEM_mallocN(len + 1, "preprocess_include");
	temp[len] = ' ';

	memcpy(temp, maindata, len);

	/* remove all c++ comments */
	/* replace all enters/tabs/etc with spaces */
	cp = temp;
	a = len;
	comment = 0;
	while (a--) {
		if (cp[0] == '/' && cp[1] == '/') {
			comment = 1;
		}
		else if (*cp == '\n') {
			comment = 0;
		}
		if (comment || *cp < 32 || *cp > 128) *cp = 32;
		cp++;
	}


	/* data from temp copy to maindata, remove comments and double spaces */
	cp = temp;
	md = maindata;
	newlen = 0;
	comment = 0;
	a = len;
	while (a--) {

		if (cp[0] == '/' && cp[1] == '*') {
			comment = 1;
			cp[0] = cp[1] = 32;
		}
		if (cp[0] == '*' && cp[1] == '/') {
			comment = 0;
			cp[0] = cp[1] = 32;
		}

		/* do not copy when: */
		if (comment) {
			/* pass */
		}
		else if (cp[0] == ' ' && cp[1] == ' ') {
			/* pass */
		}
		else if (cp[-1] == '*' && cp[0] == ' ') {
			/* pointers with a space */
		}	/* skip special keywords */
		else if (strncmp("DNA_DEPRECATED", cp, 14) == 0) {
			/* single values are skipped already, so decrement 1 less */
			a -= 13;
			cp += 13;
		}
		else if (strncmp("DNA_PRIVATE_WORKSPACE", cp, 21) == 0) {
			/* Check for DNA_PRIVATE_WORKSPACE_READ_WRITE */
			if (strncmp("_READ_WRITE", cp + 21, 11) == 0) {
				a -= 31;
				cp += 31;
			}
			else {
				a -= 20;
				cp += 20;
			}
		}
		else {
			md[0] = cp[0];
			md++;
			newlen++;
		}
		cp++;
	}

	MEM_freeN(temp);
	return newlen;
}

static void *read_file_data(const char *filename, int *r_len)
{
#ifdef WIN32
	FILE *fp = fopen(filename, "rb");
#else
	FILE *fp = fopen(filename, "r");
#endif
	void *data;

	if (!fp) {
		*r_len = -1;
		return NULL;
	}

	fseek(fp, 0L, SEEK_END);
	*r_len = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	if (*r_len == -1) {
		fclose(fp);
		return NULL;
	}

	data = MEM_mallocN(*r_len, "read_file_data");
	if (!data) {
		*r_len = -1;
		fclose(fp);
		return NULL;
	}

	if (fread(data, *r_len, 1, fp) != 1) {
		*r_len = -1;
		MEM_freeN(data);
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	return data;
}

static int convert_include(const char *filename)
{
	/* read include file, skip structs with a '#' before it.
	 * store all data in temporal arrays.
	 */
	int filelen, count, slen, type, name, strct;
	short *structpoin, *sp;
	char *maindata, *mainend, *md, *md1;
	bool skip_struct;

	md = maindata = read_file_data(filename, &filelen);
	if (filelen == -1) {
		fprintf(stderr, "Can't read file %s\n", filename);
		return 1;
	}

	filelen = preprocess_include(maindata, filelen);
	mainend = maindata + filelen - 1;

	/* we look for '{' and then back to 'struct' */
	count = 0;
	skip_struct = false;
	while (count < filelen) {

		/* code for skipping a struct: two hashes on 2 lines. (preprocess added a space) */
		if (md[0] == '#' && md[1] == ' ' && md[2] == '#') {
			skip_struct = true;
		}

		if (md[0] == '{') {
			md[0] = 0;
			if (skip_struct) {
				skip_struct = false;
			}
			else {
				if (md[-1] == ' ') md[-1] = 0;
				md1 = md - 2;
				while (*md1 != 32) md1--;       /* to beginning of word */
				md1++;

				/* we've got a struct name when... */
				if (strncmp(md1 - 7, "struct", 6) == 0) {

					strct = add_type(md1, 0);
					if (strct == -1) {
						fprintf(stderr, "File '%s' contains struct we cant parse \"%s\"\n", filename, md1);
						return 1;
					}

					structpoin = add_struct(strct);
					sp = structpoin + 2;

					if (debugSDNA > 1) printf("\t|\t|-- detected struct %s\n", types[strct]);

					/* first lets make it all nice strings */
					md1 = md + 1;
					while (*md1 != '}') {
						if (md1 > mainend) break;

						if (*md1 == ',' || *md1 == ' ') *md1 = 0;
						md1++;
					}

					/* read types and names until first character that is not '}' */
					md1 = md + 1;
					while (*md1 != '}') {
						if (md1 > mainend) break;

						/* skip when it says 'struct' or 'unsigned' or 'const' */
						if (*md1) {
							if (strncmp(md1, "struct", 6) == 0) md1 += 7;
							if (strncmp(md1, "unsigned", 8) == 0) md1 += 9;
							if (strncmp(md1, "const", 5) == 0) md1 += 6;

							/* we've got a type! */
							type = add_type(md1, 0);
							if (type == -1) {
								fprintf(stderr, "File '%s' contains struct we can't parse \"%s\"\n", filename, md1);
								return 1;
							}

							if (debugSDNA > 1) printf("\t|\t|\tfound type %s (", md1);

							md1 += strlen(md1);


							/* read until ';' */
							while (*md1 != ';') {
								if (md1 > mainend) break;

								if (*md1) {
									/* We've got a name. slen needs
									 * correction for function
									 * pointers! */
									slen = (int) strlen(md1);
									if (md1[slen - 1] == ';') {
										md1[slen - 1] = 0;


										name = add_name(md1);
										slen += additional_slen_offset;
										sp[0] = type;
										sp[1] = name;

										if ((debugSDNA > 1) && (names[name] != NULL)) printf("%s |", names[name]);

										structpoin[1]++;
										sp += 2;

										md1 += slen;
										break;
									}


									name = add_name(md1);
									slen += additional_slen_offset;

									sp[0] = type;
									sp[1] = name;
									if ((debugSDNA > 1) && (names[name] != NULL)) printf("%s ||", names[name]);

									structpoin[1]++;
									sp += 2;

									md1 += slen;
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

static int arraysize(const char *str)
{
	int a, mul = 1;
	const char *cp = NULL;

	for (a = 0; str[a]; a++) {
		if (str[a] == '[') {
			cp = &(str[a + 1]);
		}
		else if (str[a] == ']' && cp) {
			/* if 'cp' is a preprocessor definition, it will evaluate to 0,
			 * the caller needs to check for this case and throw an error */
			mul *= atoi(cp);
		}
	}

	return mul;
}

static bool check_field_alignment(int firststruct, int structtype, int type, int len,
                                  const char *name, const char *detail)
{
	bool result = true;
	if (type < firststruct && typelens_native[type] > 4 && (len % 8)) {
		fprintf(stderr, "Align 8 error (%s) in struct: %s %s (add %d padding bytes)\n",
		        detail, types[structtype], name, len % 8);
		result = false;
	}
	if (typelens_native[type] > 3 && (len % 4) ) {
		fprintf(stderr, "Align 4 error (%s) in struct: %s %s (add %d padding bytes)\n",
		        detail, types[structtype], name, len % 4);
		result = false;
	}
	if (typelens_native[type] == 2 && (len % 2) ) {
		fprintf(stderr, "Align 2 error (%s) in struct: %s %s (add %d padding bytes)\n",
		        detail, types[structtype], name, len % 2);
		result = false;
	}
	return result;
}

static int calculate_structlens(int firststruct)
{
	int unknown = nr_structs, lastunknown;
	bool dna_error = false;

	while (unknown) {
		lastunknown = unknown;
		unknown = 0;

		/* check all structs... */
		for (int a = 0; a < nr_structs; a++) {
			const short *structpoin = structs[a];
			const int    structtype = structpoin[0];

			/* when length is not known... */
			if (typelens_native[structtype] == 0) {

				const short *sp = structpoin + 2;
				int len_native = 0;
				int len_32 = 0;
				int len_64 = 0;
				bool has_pointer = false;

				/* check all elements in struct */
				for (int b = 0; b < structpoin[1]; b++, sp += 2) {
					int type = sp[0];
					const char *cp = names[sp[1]];

					int namelen = (int)strlen(cp);
					/* is it a pointer or function pointer? */
					if (cp[0] == '*' || cp[1] == '*') {
						has_pointer = 1;
						/* has the name an extra length? (array) */
						int mul = 1;
						if (cp[namelen - 1] == ']') mul = arraysize(cp);

						if (mul == 0) {
							fprintf(stderr, "Zero array size found or could not parse %s: '%.*s'\n",
							        types[structtype], namelen + 1, cp);
							dna_error = 1;
						}

						/* 4-8 aligned/ */
						if (sizeof(void *) == 4) {
							if (len_native % 4) {
								fprintf(stderr, "Align pointer error in struct (len_native 4): %s %s\n",
								        types[structtype], cp);
								dna_error = 1;
							}
						}
						else {
							if (len_native % 8) {
								fprintf(stderr, "Align pointer error in struct (len_native 8): %s %s\n",
								        types[structtype], cp);
								dna_error = 1;
							}
						}

						if (len_64 % 8) {
							fprintf(stderr, "Align pointer error in struct (len_64 8): %s %s\n",
							        types[structtype], cp);
							dna_error = 1;
						}

						len_native += sizeof(void *) * mul;
						len_32 += 4 * mul;
						len_64 += 8 * mul;

					}
					else if (cp[0] == '[') {
						/* parsing can cause names "var" and "[3]" to be found for "float var [3]" ... */
						fprintf(stderr, "Parse error in struct, invalid member name: %s %s\n",
						        types[structtype], cp);
						dna_error = 1;
					}
					else if (typelens_native[type]) {
						/* has the name an extra length? (array) */
						int mul = 1;
						if (cp[namelen - 1] == ']') mul = arraysize(cp);

						if (mul == 0) {
							fprintf(stderr, "Zero array size found or could not parse %s: '%.*s'\n",
							        types[structtype], namelen + 1, cp);
							dna_error = 1;
						}

						/* struct alignment */
						if (type >= firststruct) {
							if (sizeof(void *) == 8 && (len_native % 8) ) {
								fprintf(stderr, "Align struct error: %s %s\n",
								        types[structtype], cp);
								dna_error = 1;
							}
						}

						/* Check 2-4-8 aligned. */
						if (!check_field_alignment(firststruct, structtype, type, len_32, cp, "32 bit")) {
							dna_error = 1;
						}
						if (!check_field_alignment(firststruct, structtype, type, len_64, cp, "64 bit")) {
							dna_error = 1;
						}

						len_native += mul * typelens_native[type];
						len_32 += mul * typelens_32[type];
						len_64 += mul * typelens_64[type];

					}
					else {
						len_native = 0;
						len_32 = 0;
						len_64 = 0;
						break;
					}
				}

				if (len_native == 0) {
					unknown++;
				}
				else {
					typelens_native[structtype] = len_native;
					typelens_32[structtype] = len_32;
					typelens_64[structtype] = len_64;
					/* two ways to detect if a struct contains a pointer:
					 * has_pointer is set or len_native  doesn't match any of 32/64bit lengths*/
					if (has_pointer || len_64 != len_native || len_32 != len_native) {
						if (len_64 % 8) {
							fprintf(stderr, "Sizeerror 8 in struct: %s (add %d bytes)\n",
							        types[structtype], len_64 % 8);
							dna_error = 1;
						}
					}

					if (len_native % 4) {
						fprintf(stderr, "Sizeerror 4 in struct: %s (add %d bytes)\n",
						       types[structtype], len_native % 4);
						dna_error = 1;
					}

				}
			}
		}

		if (unknown == lastunknown) break;
	}

	if (unknown) {
		fprintf(stderr, "ERROR: still %d structs unknown\n", unknown);

		if (debugSDNA) {
			fprintf(stderr, "*** Known structs :\n");

			for (int a = 0; a < nr_structs; a++) {
				const short *structpoin = structs[a];
				const int    structtype = structpoin[0];

				/* length unknown */
				if (typelens_native[structtype] != 0) {
					fprintf(stderr, "  %s\n", types[structtype]);
				}
			}
		}


		fprintf(stderr, "*** Unknown structs :\n");

		for (int a = 0; a < nr_structs; a++) {
			const short *structpoin = structs[a];
			const int    structtype = structpoin[0];

			/* length unknown yet */
			if (typelens_native[structtype] == 0) {
				fprintf(stderr, "  %s\n", types[structtype]);
			}
		}

		dna_error = 1;
	}

	return(dna_error);
}

#define MAX_DNA_LINE_LENGTH 20

static void dna_write(FILE *file, const void *pntr, const int size)
{
	static int linelength = 0;
	int i;
	const char *data;

	data = (const char *)pntr;

	for (i = 0; i < size; i++) {
		fprintf(file, "%d, ", data[i]);
		linelength++;
		if (linelength >= MAX_DNA_LINE_LENGTH) {
			fprintf(file, "\n");
			linelength = 0;
		}
	}
}

void printStructLengths(void)
{
	int a, unknown = nr_structs, structtype;
	/*int lastunknown;*/ /*UNUSED*/
	const short *structpoin;
	printf("\n\n*** All detected structs:\n");

	while (unknown) {
		/*lastunknown = unknown;*/ /*UNUSED*/
		unknown = 0;

		/* check all structs... */
		for (a = 0; a < nr_structs; a++) {
			structpoin = structs[a];
			structtype = structpoin[0];
			printf("\t%s\t:%d\n", types[structtype], typelens_native[structtype]);
		}
	}

	printf("*** End of list\n");

}


static int make_structDNA(const char *baseDirectory, FILE *file, FILE *file_offsets)
{
	int len, i;
	const short *sp;
	/* str contains filenames. Since we now include paths, I stretched       */
	/* it a bit. Hope this is enough :) -nzc-                                */
	char str[SDNA_MAX_FILENAME_LENGTH], *cp;
	int firststruct;

	if (debugSDNA > 0) {
		fflush(stdout);
		printf("Running makesdna at debug level %d\n", debugSDNA);
	}

	/* the longest known struct is 50k, so we assume 100k is sufficent! */
	namedata = MEM_callocN(maxdata, "namedata");
	typedata = MEM_callocN(maxdata, "typedata");
	structdata = MEM_callocN(maxdata, "structdata");

	/* a maximum of 5000 variables, must be sufficient? */
	names = MEM_callocN(sizeof(char *) * maxnr, "names");
	types = MEM_callocN(sizeof(char *) * maxnr, "types");
	typelens_native = MEM_callocN(sizeof(short) * maxnr, "typelens_native");
	typelens_32 = MEM_callocN(sizeof(short) * maxnr, "typelens_32");
	typelens_64 = MEM_callocN(sizeof(short) * maxnr, "typelens_64");
	structs = MEM_callocN(sizeof(short *) * maxnr, "structs");

	/**
	 * Insertion of all known types.
	 *
	 * \warning Order of function calls here must be aligned with #eSDNA_Type.
	 * \warning uint is not allowed! use in structs an unsigned int.
	 * \warning sizes must match #DNA_elem_type_size().
	 */
	add_type("char", 1);     /* SDNA_TYPE_CHAR */
	add_type("uchar", 1);    /* SDNA_TYPE_UCHAR */
	add_type("short", 2);    /* SDNA_TYPE_SHORT */
	add_type("ushort", 2);   /* SDNA_TYPE_USHORT */
	add_type("int", 4);      /* SDNA_TYPE_INT */

	/* note, long isn't supported,
	 * these are place-holders to maintain alignment with eSDNA_Type*/
	add_type("long", 4);     /* SDNA_TYPE_LONG */
	add_type("ulong", 4);    /* SDNA_TYPE_ULONG */

	add_type("float", 4);    /* SDNA_TYPE_FLOAT */
	add_type("double", 8);   /* SDNA_TYPE_DOUBLE */
	add_type("int64_t", 8);  /* SDNA_TYPE_INT64 */
	add_type("uint64_t", 8); /* SDNA_TYPE_UINT64 */
	add_type("void", 0);     /* SDNA_TYPE_VOID */

	/* the defines above shouldn't be output in the padding file... */
	firststruct = nr_types;

	/* add all include files defined in the global array                     */
	/* Since the internal file+path name buffer has limited length, I do a   */
	/* little test first...                                                  */
	/* Mind the breaking condition here!                                     */
	if (debugSDNA) printf("\tStart of header scan:\n");
	for (i = 0; *(includefiles[i]) != '\0'; i++) {
		sprintf(str, "%s%s", baseDirectory, includefiles[i]);
		if (debugSDNA) printf("\t|-- Converting %s\n", str);
		if (convert_include(str)) {
			return (1);
		}
	}
	if (debugSDNA) printf("\tFinished scanning %d headers.\n", i);

	if (calculate_structlens(firststruct)) {
		/* error */
		return(1);
	}

	/* FOR DEBUG */
	if (debugSDNA > 1) {
		int a, b;
		/* short *elem; */
		short num_types;

		printf("nr_names %d nr_types %d nr_structs %d\n", nr_names, nr_types, nr_structs);
		for (a = 0; a < nr_names; a++) {
			printf(" %s\n", names[a]);
		}
		printf("\n");

		sp = typelens_native;
		for (a = 0; a < nr_types; a++, sp++) {
			printf(" %s %d\n", types[a], *sp);
		}
		printf("\n");

		for (a = 0; a < nr_structs; a++) {
			sp = structs[a];
			printf(" struct %s elems: %d size: %d\n", types[sp[0]], sp[1], typelens_native[sp[0]]);
			num_types  = sp[1];
			sp += 2;
			/* ? num_types was elem? */
			for (b = 0; b < num_types; b++, sp += 2) {
				printf("   %s %s\n", types[sp[0]], names[sp[1]]);
			}
		}
	}

	/* file writing */

	if (debugSDNA > 0) printf("Writing file ... ");

	if (nr_names == 0 || nr_structs == 0) {
		/* pass */
	}
	else {
		dna_write(file, "SDNA", 4);

		/* write names */
		dna_write(file, "NAME", 4);
		len = nr_names;
		dna_write(file, &len, 4);

		/* calculate size of datablock with strings */
		cp = names[nr_names - 1];
		cp += strlen(names[nr_names - 1]) + 1;         /* +1: null-terminator */
		len = (intptr_t) (cp - (char *) names[0]);
		len = (len + 3) & ~3;
		dna_write(file, names[0], len);

		/* write TYPES */
		dna_write(file, "TYPE", 4);
		len = nr_types;
		dna_write(file, &len, 4);

		/* calculate datablock size */
		cp = types[nr_types - 1];
		cp += strlen(types[nr_types - 1]) + 1;     /* +1: null-terminator */
		len = (intptr_t) (cp - (char *) types[0]);
		len = (len + 3) & ~3;

		dna_write(file, types[0], len);

		/* WRITE TYPELENGTHS */
		dna_write(file, "TLEN", 4);

		len = 2 * nr_types;
		if (nr_types & 1) len += 2;
		dna_write(file, typelens_native, len);

		/* WRITE STRUCTS */
		dna_write(file, "STRC", 4);
		len = nr_structs;
		dna_write(file, &len, 4);

		/* calc datablock size */
		sp = structs[nr_structs - 1];
		sp += 2 + 2 * (sp[1]);
		len = (intptr_t) ((char *) sp - (char *) structs[0]);
		len = (len + 3) & ~3;

		dna_write(file, structs[0], len);

		/* a simple dna padding test */
		if (0) {
			FILE *fp;
			int a;

			fp = fopen("padding.c", "w");
			if (fp == NULL) {
				/* pass */
			}
			else {

				/* add all include files defined in the global array */
				for (i = 0; *(includefiles[i]) != '\0'; i++) {
					fprintf(fp, "#include \"%s%s\"\n", baseDirectory, includefiles[i]);
				}

				fprintf(fp, "main() {\n");
				sp = typelens_native;
				sp += firststruct;
				for (a = firststruct; a < nr_types; a++, sp++) {
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

	/* write a simple enum with all structs offsets,
	 * should only be accessed via SDNA_TYPE_FROM_STRUCT macro */
	{
		fprintf(file_offsets, "#define SDNA_TYPE_FROM_STRUCT(id) _SDNA_TYPE_##id\n");
		fprintf(file_offsets, "enum {\n");
		for (i = 0; i < nr_structs; i++) {
			const short *structpoin = structs[i];
			const int    structtype = structpoin[0];
			fprintf(file_offsets, "\t_SDNA_TYPE_%s = %d,\n", types[structtype], i);
		}
		fprintf(file_offsets, "\tSDNA_TYPE_MAX = %d,\n", nr_structs);
		fprintf(file_offsets, "};\n");
	}

	MEM_freeN(namedata);
	MEM_freeN(typedata);
	MEM_freeN(structdata);
	MEM_freeN(names);
	MEM_freeN(types);
	MEM_freeN(typelens_native);
	MEM_freeN(typelens_32);
	MEM_freeN(typelens_64);
	MEM_freeN(structs);

	if (debugSDNA > 0) printf("done.\n");

	return(0);
}

/* ************************* END MAKE DNA ********************** */

static void make_bad_file(const char *file, int line)
{
	FILE *fp = fopen(file, "w");
	fprintf(fp, "#error \"Error! can't make correct DNA.c file from %s:%d, STUPID!\"\n", __FILE__, line);
	fclose(fp);
}

#ifndef BASE_HEADER
#define BASE_HEADER "../"
#endif

int main(int argc, char **argv)
{
	int return_status = 0;

	if (argc != 3 && argc != 4) {
		printf("Usage: %s dna.c dna_struct_offsets.h [base directory]\n", argv[0]);
		return_status = 1;
	}
	else {
		FILE *file_dna         = fopen(argv[1], "w");
		FILE *file_dna_offsets = fopen(argv[2], "w");
		if (!file_dna) {
			printf("Unable to open file: %s\n", argv[1]);
			return_status = 1;
		}
		else if (!file_dna_offsets) {
			printf("Unable to open file: %s\n", argv[2]);
			return_status = 1;
		}
		else {
			const char *baseDirectory;

			if (argc == 4) {
				baseDirectory = argv[3];
			}
			else {
				baseDirectory = BASE_HEADER;
			}

			fprintf(file_dna, "const unsigned char DNAstr[] = {\n");
			if (make_structDNA(baseDirectory, file_dna, file_dna_offsets)) {
				/* error */
				fclose(file_dna);
				file_dna = NULL;
				make_bad_file(argv[1], __LINE__);
				return_status = 1;
			}
			else {
				fprintf(file_dna, "};\n");
				fprintf(file_dna, "const int DNAlen = sizeof(DNAstr);\n");
			}
		}

		if (file_dna) {
			fclose(file_dna);
		}
		if (file_dna_offsets) {
			fclose(file_dna_offsets);
		}
	}


	return(return_status);
}

/* handy but fails on struct bounds which makesdna doesnt care about
 * with quite the same strictness as GCC does */
#if 0
/* include files for automatic dependencies */

/* extra safety check that we are aligned,
 * warnings here are easier to fix the makesdna's */
#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wpadded"
#endif

#endif /* if 0 */

/* even though DNA supports, 'long' shouldn't be used since it can be either 32 or 64bit,
 * use int or int64_t instead.
 * Only valid use would be as a runtime variable if an API expected a long,
 * but so far we dont have this happening. */
#ifdef __GNUC__
#  pragma GCC poison long
#endif

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
#include "DNA_object_force_types.h"
#include "DNA_object_fluidsim_types.h"
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
#include "DNA_gpencil_modifier_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_anim_types.h"
#include "DNA_boid_types.h"
#include "DNA_smoke_types.h"
#include "DNA_speaker_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_mask_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_layer_types.h"
#include "DNA_workspace_types.h"
#include "DNA_lightprobe_types.h"

/* end of list */
