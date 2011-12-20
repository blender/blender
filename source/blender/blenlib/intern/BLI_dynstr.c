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
 * Dynamically sized string ADT
 */

/** \file blender/blenlib/intern/BLI_dynstr.c
 *  \ingroup bli
 */


#include <stdarg.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#ifdef _WIN32
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#endif

#ifndef va_copy
# ifdef __va_copy
#  define va_copy(a,b) __va_copy(a,b)
# else /* !__va_copy */
#  define va_copy(a,b) ((a)=(b))
# endif /* __va_copy */
#endif /* va_copy */

/***/

typedef struct DynStrElem DynStrElem;
struct DynStrElem {
	DynStrElem *next;
	
	char *str;
};

struct DynStr {
	DynStrElem *elems, *last;
	int curlen;
};

/***/

DynStr *BLI_dynstr_new(void)
{
	DynStr *ds= MEM_mallocN(sizeof(*ds), "DynStr");
	ds->elems= ds->last= NULL;
	ds->curlen= 0;
	
	return ds;
}

void BLI_dynstr_append(DynStr *ds, const char *cstr)
{
	DynStrElem *dse= malloc(sizeof(*dse));
	int cstrlen= strlen(cstr);
	
	dse->str= malloc(cstrlen+1);
	memcpy(dse->str, cstr, cstrlen+1);
	dse->next= NULL;
	
	if (!ds->last)
		ds->last= ds->elems= dse;
	else
		ds->last= ds->last->next= dse;

	ds->curlen+= cstrlen;
}

void BLI_dynstr_nappend(DynStr *ds, const char *cstr, int len)
{
	DynStrElem *dse= malloc(sizeof(*dse));
	int cstrlen= BLI_strnlen(cstr, len);

	dse->str= malloc(cstrlen+1);
	memcpy(dse->str, cstr, cstrlen);
	dse->str[cstrlen] = '\0';
	dse->next= NULL;

	if (!ds->last)
		ds->last= ds->elems= dse;
	else
		ds->last= ds->last->next= dse;

	ds->curlen+= cstrlen;
}

void BLI_dynstr_vappendf(DynStr *ds, const char *format, va_list args)
{
	char *message, fixedmessage[256];
	int len= sizeof(fixedmessage);
	const int maxlen= 65536;
	int retval;

	while(1) {
		va_list args_cpy;
		if(len == sizeof(fixedmessage))
			message= fixedmessage;
		else
			message= MEM_callocN(sizeof(char) * len, "BLI_dynstr_appendf");

		/* cant reuse the same args, so work on a copy */
		va_copy(args_cpy, args);
		retval= vsnprintf(message, len, format, args_cpy);
		va_end(args_cpy);

		if(retval == -1) {
			/* -1 means not enough space, but on windows it may also mean
			 * there is a formatting error, so we impose a maximum length */
			if(message != fixedmessage)
				MEM_freeN(message);
			message= NULL;

			len *= 2;
			if(len > maxlen) {
				fprintf(stderr, "BLI_dynstr_append text too long or format error.\n");
				break;
			}
		}
		else if(retval >= len) {
			/* in C99 the actual length required is returned */
			if(message != fixedmessage)
				MEM_freeN(message);
			message= NULL;

			/* retval doesnt include \0 terminator */
			len= retval + 1;
		}
		else
			break;
	}

	if(message) {
		BLI_dynstr_append(ds, message);

		if(message != fixedmessage)
			MEM_freeN(message);
	}
}

void BLI_dynstr_appendf(DynStr *ds, const char *format, ...)
{
	va_list args;
	char *message, fixedmessage[256];
	int len= sizeof(fixedmessage);
	const int maxlen= 65536;
	int retval;

	/* note that it's tempting to just call BLI_dynstr_vappendf here
	 * and avoid code duplication, that crashes on some system because
	 * va_start/va_end have to be called for each vsnprintf call */

	while(1) {
		if(len == sizeof(fixedmessage))
			message= fixedmessage;
		else
			message= MEM_callocN(sizeof(char)*(len), "BLI_dynstr_appendf");

		va_start(args, format);
		retval= vsnprintf(message, len, format, args);
		va_end(args);

		if(retval == -1) {
			/* -1 means not enough space, but on windows it may also mean
			 * there is a formatting error, so we impose a maximum length */
			if(message != fixedmessage)
				MEM_freeN(message);
			message= NULL;

			len *= 2;
			if(len > maxlen) {
				fprintf(stderr, "BLI_dynstr_append text too long or format error.\n");
				break;
			}
		}
		else if(retval >= len) {
			/* in C99 the actual length required is returned */
			if(message != fixedmessage)
				MEM_freeN(message);
			message= NULL;

			/* retval doesnt include \0 terminator */
			len= retval + 1;
		}
		else
			break;
	}

	if(message) {
		BLI_dynstr_append(ds, message);

		if(message != fixedmessage)
			MEM_freeN(message);
	}
}

int BLI_dynstr_get_len(DynStr *ds)
{
	return ds->curlen;
}

void BLI_dynstr_get_cstring_ex(DynStr *ds, char *rets)
{
	char *s;
	DynStrElem *dse;

	for (s= rets, dse= ds->elems; dse; dse= dse->next) {
		int slen= strlen(dse->str);

		memcpy(s, dse->str, slen);

		s+= slen;
	}
	rets[ds->curlen]= '\0';
}

char *BLI_dynstr_get_cstring(DynStr *ds)
{
	char *rets= MEM_mallocN(ds->curlen+1, "dynstr_cstring");
	BLI_dynstr_get_cstring_ex(ds, rets);
	return rets;
}

void BLI_dynstr_free(DynStr *ds)
{
	DynStrElem *dse;
	
	for (dse= ds->elems; dse; ) {
		DynStrElem *n= dse->next;
		
		free(dse->str);
		free(dse);
		
		dse= n;
	}
	
	MEM_freeN(ds);
}
