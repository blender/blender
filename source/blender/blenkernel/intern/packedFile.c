/**
 * blenkernel/packedFile.c - (cleaned up mar-01 nzc)
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
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif
#include <string.h>
#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_sound_types.h"
#include "DNA_vfont_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_sound.h"
#include "BKE_image.h"
#include "BKE_font.h"
#include "BKE_packedFile.h"
#include "BKE_bad_level_calls.h" /* <- waitcursor */

int seekPackedFile(PackedFile * pf, int offset, int whence)
{
	int oldseek = -1, seek = 0;

	if (pf) {
		oldseek = pf->seek;
		switch(whence) {
		case SEEK_CUR:
			seek = oldseek + offset;
			break;
		case SEEK_END:
			seek = pf->size + offset;
			break;
		case SEEK_SET:
			seek = offset;
			break;
		default:
			oldseek = -1;
		}
		if (seek < 0) {
			seek = 0;
		} else if (seek > pf->size) {
			seek = pf->size;
		}
		pf->seek = seek;
	}

	return(oldseek);
}
	
void rewindPackedFile(PackedFile * pf)
{
	seekPackedFile(pf, 0, SEEK_SET);
}

int readPackedFile(PackedFile * pf, void * data, int size)
{ 
	if ((pf != NULL) && (size >= 0) && (data != NULL)) {
		if (size + pf->seek > pf->size) {
			size = pf->size - pf->seek;
		}

		if (size > 0) {
			memcpy(data, ((char *) pf->data) + pf->seek, size);
		} else {
			size = 0;
		}

		pf->seek += size;
	} else {
		size = -1;
	}

	return(size);
}

int countPackedFiles()
{
	int count = 0;
	Image *ima;
	VFont *vf;
	bSample *sample;
	
	// let's check if there are packed files...
	ima = G.main->image.first;
	while (ima) {
		if (ima->packedfile) {
			count++;
		}
		ima= ima->id.next;
	}

	vf = G.main->vfont.first;
	while (vf) {
		if (vf->packedfile) {
			count++;
		}
		vf = vf->id.next;
	}

	sample = samples->first;
	while (sample) {
		if (sample->packedfile) {
			count++;
		}
		sample = sample->id.next;
	}

	return(count);
}

void freePackedFile(PackedFile * pf)
{
	if (pf) {
		MEM_freeN(pf->data);
		MEM_freeN(pf);
	} else {
		printf("freePackedFile: Trying to free a NULL pointer\n");
	}
}
	
PackedFile * newPackedFileMemory(void *mem, int memlen)
{
	PackedFile * pf = MEM_callocN(sizeof(*pf), "PackedFile");
	pf->data = mem;
	pf->size = memlen;
	
	return pf;
}

PackedFile * newPackedFile(char * filename)
{
	PackedFile * pf = NULL;
	int file, filelen;
	char name[FILE_MAXDIR+FILE_MAXFILE];
	void * data;
	
	waitcursor(1);
	
	// convert relative filenames to absolute filenames
	
	strcpy(name, filename);
	BLI_convertstringcode(name, G.sce);
	
	// open the file
	// and create a PackedFile structure

	file= open(name, O_BINARY|O_RDONLY);
	if (file <= 0) {
		// error("Can't open file: %s", name);
	} else {
		filelen = BLI_filesize(file);

		if (filelen == 0) {
			// MEM_mallocN complains about MEM_mallocN(0, "bla");
			// we don't care....
			data = MEM_mallocN(1, "packFile");
		} else {
			data = MEM_mallocN(filelen, "packFile");
		}
		if (read(file, data, filelen) == filelen) {
			pf = newPackedFileMemory(data, filelen);
		}

		close(file);
	}

	waitcursor(0);
		
	return (pf);
}

void packAll()
{
	Image *ima;
	VFont *vf;
	bSample *sample;
	
	ima = G.main->image.first;
	while (ima) {
		if (ima->packedfile == NULL) {
			ima->packedfile = newPackedFile(ima->name);
		}
		ima= ima->id.next;
	}
	
	vf = G.main->vfont.first;
	while (vf) {
		if (vf->packedfile == NULL) {
			vf->packedfile = newPackedFile(vf->name);
		}
		vf = vf->id.next;
	}


	sample = samples->first;
	while (sample) {
		if (sample->packedfile == NULL) {
			sound_set_packedfile(sample, newPackedFile(sample->name));
		}
		sample = sample->id.next;
	}
}


/*

// attempt to create a function that generates an unique filename
// this will work when all funtions in fileops.c understand relative filenames...

char * find_new_name(char * name)
{
	char tempname[FILE_MAXDIR + FILE_MAXFILE];
	char * newname;
	
	if (fop_exists(name)) {
		for (number = 1; number <= 999; number++) {
			sprintf(tempname, "%s.%03d", name, number);
			if (! fop_exists(tempname)) {
				break;
			}
		}
	}
	
	newname = mallocN(strlen(tempname) + 1, "find_new_name");
	strcpy(newname, tempname);
	
	return(newname);
}
	
*/

int writePackedFile(char * filename, PackedFile *pf, int guimode)
{
	int file, number, remove_tmp = FALSE;
	int ret_value = RET_OK;
	char name[FILE_MAXDIR + FILE_MAXFILE];
	char tempname[FILE_MAXDIR + FILE_MAXFILE];
/*  	void * data; */
	
	if (guimode) waitcursor(1);
	
	strcpy(name, filename);
	BLI_convertstringcode(name, G.sce);
	
	if (BLI_exists(name)) {
		for (number = 1; number <= 999; number++) {
			sprintf(tempname, "%s.%03d_", name, number);
			if (! BLI_exists(tempname)) {
				if (BLI_copy_fileops(name, tempname) == RET_OK) {
					remove_tmp = TRUE;
				}
				break;
			}
		}
	}
	
	// make sure the path to the file exists...
	BLI_make_existing_file(name);
	
	file = open(name, O_BINARY + O_WRONLY + O_CREAT + O_TRUNC, 0666);
	if (file >= 0) {
		if (write(file, pf->data, pf->size) != pf->size) {
			if(guimode) error("Error writing file: %s", name);
			ret_value = RET_ERROR;
		}
		close(file);
	} else {
		if(guimode) error("Error creating file: %s", name);
		ret_value = RET_ERROR;
	}
	
	if (remove_tmp) {
		if (ret_value == RET_ERROR) {
			if (BLI_rename(tempname, name) != 0) {
				if(guimode) error("Error restoring tempfile. Check files: '%s' '%s'", tempname, name);
			}
		} else {
			if (BLI_delete(tempname, 0, 0) != 0) {
				if(guimode) error("Error deleting '%s' (ignored)");
			}
		}
	}
	
	if(guimode) waitcursor(0);

	return (ret_value);
}
	
/* 

This function compares a packed file to a 'real' file.
It returns an integer indicating if:

PF_EQUAL		- the packed file and original file are identical
PF_DIFFERENT	- the packed file and original file differ
PF_NOFILE		- the original file doens't exist

*/

int checkPackedFile(char * filename, PackedFile * pf)
{
	struct stat st;
	int ret_val, i, len, file;
	char buf[4096];
	char name[FILE_MAXDIR + FILE_MAXFILE];
	
	strcpy(name, filename);
	BLI_convertstringcode(name, G.sce);
	
	if (stat(name, &st)) {
		ret_val = PF_NOFILE;
	} else if (st.st_size != pf->size) {
		ret_val = PF_DIFFERS;
	} else {
		// we'll have to compare the two...
		
		file = open(name, O_BINARY | O_RDONLY);
		if (file < 0) {
			ret_val = PF_NOFILE;
		} else {
			ret_val = PF_EQUAL;
			
			for (i = 0; i < pf->size; i += sizeof(buf)) {
				len = pf->size - i;
				if (len > sizeof(buf)) {
					len = sizeof(buf);
				}
				
				if (read(file, buf, len) != len) {
					// read error ...
					ret_val = PF_DIFFERS;
					break;
				} else {
					if (memcmp(buf, ((char *)pf->data) + i, len)) {
						ret_val = PF_DIFFERS;
						break;
					}
				}
			}
		}
	}
	
	return(ret_val);
}

/*

unpackFile() looks at the existing files (abs_name, local_name) and a packed file.
If how == PF_ASK it offers the user a couple of options what to do with the packed file.

It returns a char * to the existing file name / new file name or NULL when
there was an error or when the user desides to cancel the operation.

*/

char *unpackFile(char * abs_name, char * local_name, PackedFile * pf, int how)
{
	char menu[6 * (FILE_MAXDIR + FILE_MAXFILE + 100)];
	char line[FILE_MAXDIR + FILE_MAXFILE + 100];
	char * newname = NULL, * temp = NULL;
	
	// char newabs[FILE_MAXDIR + FILE_MAXFILE];
	// char newlocal[FILE_MAXDIR + FILE_MAXFILE];
	
	if (pf != NULL) {
		if (how == PF_ASK) {
			sprintf(menu, "UnPack file%%t|Remove Pack %%x%d", PF_REMOVE);
			
			if (strcmp(abs_name, local_name)) {
				switch (checkPackedFile(local_name, pf)) {
					case PF_NOFILE:
						sprintf(line, "|Create %s%%x%d", local_name, PF_WRITE_LOCAL);
						strcat(menu, line);
						break;
					case PF_EQUAL:
						sprintf(line, "|Use %s (identical)%%x%d", local_name, PF_USE_LOCAL);
						strcat(menu, line);
						break;
					case PF_DIFFERS:
						sprintf(line, "|Use %s (differs)%%x%d", local_name, PF_USE_LOCAL);
						strcat(menu, line);
						sprintf(line, "|Overwrite %s%%x%d", local_name, PF_WRITE_LOCAL);
						strcat(menu, line);
						break;
				}
				// sprintf(line, "|%%x%d", PF_INVALID);
				// strcat(menu, line);
			}
			
			switch (checkPackedFile(abs_name, pf)) {
				case PF_NOFILE:
					sprintf(line, "|Create %s%%x%d", abs_name, PF_WRITE_ORIGINAL);
					strcat(menu, line);
					break;
				case PF_EQUAL:
					sprintf(line, "|Use %s (identical)%%x%d", abs_name, PF_USE_ORIGINAL);
					strcat(menu, line);
					break;
				case PF_DIFFERS:
					sprintf(line, "|Use %s (differs)%%x%d", abs_name, PF_USE_ORIGINAL);
					strcat(menu, line);
					sprintf(line, "|Overwrite %s%%x%d", abs_name, PF_WRITE_ORIGINAL);
					strcat(menu, line);
					break;
			}
			
			how = pupmenu(menu);
		}
		
		switch (how) {
			case -1:
			case PF_KEEP:
				break;
			case PF_REMOVE:
				temp= abs_name;
				break;
			case PF_USE_LOCAL:
				// if file exists use it
				if (BLI_exists(local_name)) {
					temp = local_name;
					break;
				}
				// else fall through and create it
			case PF_WRITE_LOCAL:
				if (writePackedFile(local_name, pf, 1) == RET_OK) {
					temp = local_name;
				}
				break;
			case PF_USE_ORIGINAL:
				// if file exists use it
				if (BLI_exists(abs_name)) {
					temp = abs_name;
					break;
				}
				// else fall through and create it
			case PF_WRITE_ORIGINAL:
				if (writePackedFile(abs_name, pf, 1) == RET_OK) {
					temp = abs_name;
				}
				break;
			default:
				printf("unpackFile: unknown return_value %d\n", how);
				break;
		}
		
		if (temp) {
			newname = MEM_mallocN(strlen(temp) + 1, "unpack_file newname");
			strcpy(newname, temp);
		}
	}
	
	return (newname);
}


int unpackVFont(VFont * vfont, int how)
{
	char localname[FILE_MAXDIR + FILE_MAXFILE], fi[FILE_MAXFILE];
	char * newname;
	int ret_value = RET_ERROR;
	
	if (vfont != NULL) {
		strcpy(localname, vfont->name);
		BLI_splitdirstring(localname, fi);
		
		sprintf(localname, "//fonts/%s", fi);
		
		newname = unpackFile(vfont->name, localname, vfont->packedfile, how);
		if (newname != NULL) {
			ret_value = RET_OK;
			freePackedFile(vfont->packedfile);
			vfont->packedfile = 0;
			strcpy(vfont->name, newname);
			MEM_freeN(newname);
		}
	}
	
	return (ret_value);
}

int unpackSample(bSample *sample, int how)
{
	char localname[FILE_MAXDIR + FILE_MAX], fi[FILE_MAX];
	char * newname;
	int ret_value = RET_ERROR;
	PackedFile *pf;
	
	if (sample != NULL) {
		strcpy(localname, sample->name);
		BLI_splitdirstring(localname, fi);
		sprintf(localname, "//samples/%s", fi);
		
		newname = unpackFile(sample->name, localname, sample->packedfile, how);
		if (newname != NULL) {
			strcpy(sample->name, newname);
			MEM_freeN(newname);

			pf = sample->packedfile;
			// because samples and sounds can point to the
			// same packedfile we have to check them all
			sound_set_packedfile(sample, NULL);
			freePackedFile(pf);

			ret_value = RET_OK;
		}
	}
	
	return(ret_value);
}

int unpackImage(Image * ima, int how)
{
	char localname[FILE_MAXDIR + FILE_MAX], fi[FILE_MAX];
	char * newname;
	int ret_value = RET_ERROR;
	
	if (ima != NULL) {
		strcpy(localname, ima->name);
		BLI_splitdirstring(localname, fi);
		sprintf(localname, "//textures/%s", fi);
			
		newname = unpackFile(ima->name, localname, ima->packedfile, how);
		if (newname != NULL) {
			ret_value = RET_OK;
			freePackedFile(ima->packedfile);
			ima->packedfile = NULL;
			strcpy(ima->name, newname);
			MEM_freeN(newname);
			BKE_image_signal(ima, NULL, IMA_SIGNAL_RELOAD);
		}
	}
	
	return(ret_value);
}

void unpackAll(int how)
{
	Image *ima;
	VFont *vf;
	bSample *sample;
		
	ima = G.main->image.first;
	while (ima) {
		if (ima->packedfile) {
			unpackImage(ima, how);
		}
		ima= ima->id.next;
	}
	
	vf = G.main->vfont.first;
	while (vf) {
		if (vf->packedfile) {
			unpackVFont(vf, how);
		}
		vf = vf->id.next;
	}

	sample = samples->first;
	while (sample) {
		if (sample->packedfile) {
			unpackSample(sample, how);
		}
		sample = sample->id.next;
	}
}
