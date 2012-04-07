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

/** \file DNA_genfile.h
 *  \ingroup DNA
 *  \brief blenloader genfile private function prototypes
 */

#ifndef __DNA_GENFILE_H__
#define __DNA_GENFILE_H__

struct SDNA;

extern unsigned char DNAstr[];  /* DNA.c */
extern int DNAlen;

typedef enum eSDNA_Type {
	SDNA_TYPE_CHAR     = 0,
	SDNA_TYPE_UCHAR    = 1,
	SDNA_TYPE_SHORT    = 2,
	SDNA_TYPE_USHORT   = 3,
	SDNA_TYPE_INT      = 4,
	SDNA_TYPE_LONG     = 5,
	SDNA_TYPE_ULONG    = 6,
	SDNA_TYPE_FLOAT    = 7,
	SDNA_TYPE_DOUBLE   = 8,
	/* ,SDNA_TYPE_VOID = 9 */ /* nothing uses yet */
	SDNA_TYPE_INT64    = 10,
	SDNA_TYPE_UINT64   = 11
} eSDNA_Type;

/* define so switch statements don't complain */
#define SDNA_TYPE_VOID 9

struct SDNA *DNA_sdna_from_data(void *data, int datalen, int do_endian_swap);
void DNA_sdna_free(struct SDNA *sdna);

int DNA_struct_find_nr(struct SDNA *sdna, const char *str);
void DNA_struct_switch_endian(struct SDNA *oldsdna, int oldSDNAnr, char *data);
char *DNA_struct_get_compareflags(struct SDNA *sdna, struct SDNA *newsdna);
void *DNA_struct_reconstruct(struct SDNA *newsdna, struct SDNA *oldsdna, char *compflags, int oldSDNAnr, int blocks, void *data);

int DNA_elem_array_size(const char *astr, int len);
int DNA_elem_offset(struct SDNA *sdna, const char *stype, const char *vartype, const char *name);


int DNA_elem_type_size(const eSDNA_Type elem_nr);

#endif /* __DNA_GENFILE_H__ */
