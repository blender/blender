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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file, with exception of below:
 *
 * Contributor(s): Peter O'Gorman
 * The functions osxdlopen() and osxerror() 
 * are Copyright (c) 2002 Peter O'Gorman <ogorman@users.sourceforge.net>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../PIL_dynlib.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(CHAR_MAX)
#define CHAR_MAX 255
#endif

/*
 * XXX, should use mallocN so we can see
 * handle's not being released. fixme zr
 */
 
#ifdef WIN32

#include <windows.h>

struct PILdynlib {
	void *handle;
};

PILdynlib *PIL_dynlib_open(char *name) {
	void *handle= LoadLibrary(name);

	if (handle) {	
		PILdynlib *lib= malloc(sizeof(*lib));
		lib->handle= handle;
		
		return lib;
	} else {
		return NULL;
	}
}

void *PIL_dynlib_find_symbol(PILdynlib* lib, char *symname) {
	return GetProcAddress(lib->handle, symname);
}

char *PIL_dynlib_get_error_as_string(PILdynlib* lib) {
	int err;

	/* if lib is NULL reset the last error code */
	err= GetLastError();
	if (!lib) SetLastError(ERROR_SUCCESS);

	if (err) {
		static char buf[1024];

		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 
					NULL, 
					err, 
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
					buf, 
					sizeof(buf), 
					NULL))
			return buf;
	}
	
	return err;
}

void PIL_dynlib_close(PILdynlib *lib) {
	FreeLibrary(lib->handle);
	
	free(lib);
}

#else
#ifdef __APPLE__	/* MacOS X */

#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <stdarg.h>

#define ERR_STR_LEN 256

struct PILdynlib {
	void *handle;
};

static char *osxerror(int setget, const char *str, ...)
{
	static char errstr[ERR_STR_LEN];
	static int err_filled = 0;
	char *retval;
	NSLinkEditErrors ler;
	int lerno;
	const char *dylderrstr;
	const char *file;
	va_list arg;
	if (setget <= 0)
	{
		va_start(arg, str);
		strncpy(errstr, "dlsimple: ", ERR_STR_LEN);
		vsnprintf(errstr + 10, ERR_STR_LEN - 10, str, arg);
		va_end(arg);
	/* We prefer to use the dyld error string if setget is 0 */
		if (setget == 0) {
			NSLinkEditError(&ler, &lerno, &file, &dylderrstr);
//			printf("dyld: %s\n",dylderrstr);
			if (dylderrstr && strlen(dylderrstr))
				strncpy(errstr,dylderrstr,ERR_STR_LEN);
		}		
		err_filled = 1;
		retval = NULL;
	}
	else
	{
		if (!err_filled)
			retval = NULL;
		else
			retval = errstr;
		err_filled = 0;
	}
	return retval;
}

static void *osxdlopen(const char *path, int mode)
{
	void *module = 0;
	NSObjectFileImage ofi = 0;
	NSObjectFileImageReturnCode ofirc;
	static int (*make_private_module_public) (NSModule module) = 0;
	unsigned int flags =  NSLINKMODULE_OPTION_RETURN_ON_ERROR | NSLINKMODULE_OPTION_PRIVATE;

	/* If we got no path, the app wants the global namespace, use -1 as the marker
	   in this case */
	if (!path)
		return (void *)-1;

	/* Create the object file image, works for things linked with the -bundle arg to ld */
	ofirc = NSCreateObjectFileImageFromFile(path, &ofi);
	switch (ofirc)
	{
		case NSObjectFileImageSuccess:
			/* It was okay, so use NSLinkModule to link in the image */
			if (!(mode & RTLD_LAZY)) flags += NSLINKMODULE_OPTION_BINDNOW;
			module = NSLinkModule(ofi, path,flags);
			/* Don't forget to destroy the object file image, unless you like leaks */
			NSDestroyObjectFileImage(ofi);
			/* If the mode was global, then change the module, this avoids
			   multiply defined symbol errors to first load private then make
			   global. Silly, isn't it. */
			if ((mode & RTLD_GLOBAL))
			{
			  if (!make_private_module_public)
			  {
			    _dyld_func_lookup("__dyld_NSMakePrivateModulePublic", 
				(unsigned long *)&make_private_module_public);
			  }
			  make_private_module_public(module);
			}
			break;
		case NSObjectFileImageInappropriateFile:
			/* It may have been a dynamic library rather than a bundle, try to load it */
			module = (void *)NSAddImage(path, NSADDIMAGE_OPTION_RETURN_ON_ERROR);
			break;
		case NSObjectFileImageFailure:
			osxerror(0,"Object file setup failure :  \"%s\"", path);
			return 0;
		case NSObjectFileImageArch:
			osxerror(0,"No object for this architecture :  \"%s\"", path);
			return 0;
		case NSObjectFileImageFormat:
			osxerror(0,"Bad object file format :  \"%s\"", path);
			return 0;
		case NSObjectFileImageAccess:
			osxerror(0,"Can't read object file :  \"%s\"", path);
			return 0;		
	}
	if (!module)
		osxerror(0, "Can not open \"%s\"", path);
	return module;
}

PILdynlib *PIL_dynlib_open(char *name) {
	void *handle= osxdlopen(name, RTLD_LAZY);

	if (handle) {	
		PILdynlib *lib= malloc(sizeof(*lib));
		lib->handle= handle;
		
		return lib;
	} else {
		return NULL;
	}
}

void *PIL_dynlib_find_symbol(PILdynlib* lib, char *symname) 
{
	int sym_len = strlen(symname);
	void *value = NULL;
	char *malloc_sym = NULL;
	NSSymbol *nssym = 0;
	malloc_sym = malloc(sym_len + 2);
	if (malloc_sym)
	{
		sprintf(malloc_sym, "_%s", symname);
		/* If the lib->handle is -1, if is the app global context */
		if (lib->handle == (void *)-1)
		{
			/* Global context, use NSLookupAndBindSymbol */
			if (NSIsSymbolNameDefined(malloc_sym))
			{
				nssym = NSLookupAndBindSymbol(malloc_sym);
			}
		}
		/* Now see if the lib->handle is a struch mach_header* or not, use NSLookupSymbol in image
		   for libraries, and NSLookupSymbolInModule for bundles */
		else
		{
			/* Check for both possible magic numbers depending on x86/ppc byte order */
			if ((((struct mach_header *)lib->handle)->magic == MH_MAGIC) ||
				(((struct mach_header *)lib->handle)->magic == MH_CIGAM))
			{
				if (NSIsSymbolNameDefinedInImage((struct mach_header *)lib->handle, malloc_sym))
				{
					nssym = NSLookupSymbolInImage((struct mach_header *)lib->handle,
												  malloc_sym,
												  NSLOOKUPSYMBOLINIMAGE_OPTION_BIND
												  | NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
				}
	
			}
			else
			{
				nssym = NSLookupSymbolInModule(lib->handle, malloc_sym);
			}
		}
		if (!nssym)
		{
			osxerror(0, "symname \"%s\" Not found", symname);
		}
		value = NSAddressOfSymbol(nssym);
		free(malloc_sym);
	}
	else
	{
		osxerror(-1, "Unable to allocate memory");
	}
	return value;
}

char *PIL_dynlib_get_error_as_string(PILdynlib* lib) 
{
	return osxerror(1, (char *)NULL);
}
	
void PIL_dynlib_close(PILdynlib *lib) 
{
	if ((((struct mach_header *)lib->handle)->magic == MH_MAGIC) ||
		(((struct mach_header *)lib->handle)->magic == MH_CIGAM))
	{
		osxerror(-1, "Can't remove dynamic libraries on darwin");
	}
	if (!NSUnLinkModule(lib->handle, 0))
	{
		osxerror(0, "unable to unlink module %s", NSNameOfModule(lib->handle));
	}
	
	free(lib);
}

#else	/* Unix */

#include <dlfcn.h>

struct PILdynlib {
	void *handle;
};

PILdynlib *PIL_dynlib_open(char *name) {
	void *handle= dlopen(name, RTLD_LAZY);

	if (handle) {	
		PILdynlib *lib= malloc(sizeof(*lib));
		lib->handle= handle;
		
		return lib;
	} else {
		return NULL;
	}
}

void *PIL_dynlib_find_symbol(PILdynlib* lib, char *symname) {
	return dlsym(lib->handle, symname);
}

char *PIL_dynlib_get_error_as_string(PILdynlib* lib) {
	return dlerror();
}
	
void PIL_dynlib_close(PILdynlib *lib) {
	dlclose(lib->handle);
	
	free(lib);
}

#endif
#endif
