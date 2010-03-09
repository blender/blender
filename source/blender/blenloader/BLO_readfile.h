/*
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
 * external readfile function prototypes
 */
#ifndef BLO_READFILE_H
#define BLO_READFILE_H

#ifdef __cplusplus
extern "C" {
#endif

struct bScreen;
struct direntry;
struct LinkNode;
struct Main;
struct MemFile;
struct ReportList;
struct Scene;
struct SpaceFile;
struct SpaceImaSel;
struct UserDef;
struct bContext;

typedef struct BlendHandle	BlendHandle;

typedef enum BlenFileType {
	BLENFILETYPE_BLEND= 1, 
	BLENFILETYPE_PUB= 2, 
	BLENFILETYPE_RUNTIME= 3
} BlenFileType;

typedef struct BlendFileData {
	struct Main*	main;
	struct UserDef*	user;

	int winpos;
	int fileflags;
	int displaymode;
	int globalf;
	char filename[240];	/* 240 = FILE_MAX */
	
	struct bScreen*	curscreen;
	struct Scene*	curscene;
	
	BlenFileType	type;
} BlendFileData;

	/**
	 * Open a blender file from a pathname. The function
	 * returns NULL and sets a report in the list if
	 * it cannot open the file.
	 * 
	 * @param file The path of the file to open.
	 * @param reports If the return value is NULL, errors
	 * indicating the cause of the failure.
	 * @return The data of the file.
	 */
BlendFileData*	BLO_read_from_file		(char *file, struct ReportList *reports);

	/**
	 * Open a blender file from memory. The function
	 * returns NULL and sets a report in the list if
	 * it cannot open the file.
	 * 
	 * @param mem The file data.
	 * @param memsize The length of @a mem.
	 * @param reports If the return value is NULL, errors
	 * indicating the cause of the failure.
	 * @return The data of the file.
	 */
BlendFileData*	BLO_read_from_memory(void *mem, int memsize, struct ReportList *reports);

/**
 * oldmain is old main, from which we will keep libraries, images, ..
 * file name is current file, only for retrieving library data */

BlendFileData *BLO_read_from_memfile(struct Main *oldmain, const char *filename, struct MemFile *memfile, struct ReportList *reports);

/**
 * Free's a BlendFileData structure and _all_ the
 * data associated with it (the userdef data, and
 * the main libblock data).
 * 
 * @param bfd The structure to free.
 */
	void
BLO_blendfiledata_free(
	BlendFileData *bfd);

/**
 * Convert an idcode into a name.
 * 
 * @param code The code to convert.
 * @return A static string representing the name of
 * the code.
 */
	char*
BLO_idcode_to_name(
	int code);

/**
 * Convert a name into an idcode (ie. ID_SCE)
 * 
 * @param name The name to convert.
 * @return The code for the name, or 0 if invalid.
 */
	int
BLO_idcode_from_name(
	char *name);
	
/**
 * Open a blendhandle from a file path.
 * 
 * @param file The file path to open.
 * @return A handle on success, or NULL on failure.
 */
	BlendHandle*
BLO_blendhandle_from_file(
	char *file);

/**
 * Gets the names of all the datablocks in a file
 * of a certain type (ie. All the scene names in
 * a file).
 * 
 * @param bh The blendhandle to access.
 * @param ofblocktype The type of names to get.
 * @return A BLI_linklist of strings. The string links
 * should be freed with malloc.
 */
	struct LinkNode*
BLO_blendhandle_get_datablock_names(
	BlendHandle *bh, 
	int ofblocktype);

/**
 * Gets the previews of all the datablocks in a file
 * of a certain type (ie. All the scene names in
 * a file).
 * 
 * @param bh The blendhandle to access.
 * @param ofblocktype The type of names to get.
 * @return A BLI_linklist of PreviewImage. The PreviewImage links
 * should be freed with malloc.
 */
	struct LinkNode*
BLO_blendhandle_get_previews(
	BlendHandle *bh, 
	int ofblocktype);

/**
 * Gets the names of all the datablock groups in a
 * file. (ie. file contains Scene, Mesh, and Lamp
 * datablocks).
 * 
 * @param bh The blendhandle to access.
 * @return A BLI_linklist of strings. The string links
 * should be freed with malloc.
 */
	struct LinkNode*
BLO_blendhandle_get_linkable_groups(
	BlendHandle *bh);

/**
 * Close and free a blendhandle. The handle
 * becomes invalid after this call.
 *
 * @param bh The handle to close.
 */
	void
BLO_blendhandle_close(
	BlendHandle *bh);
	
	/***/

#define GROUP_MAX 32

int BLO_has_bfile_extension(char *str);

/* return ok when a blenderfile, in dir is the filename,
 * in group the type of libdata
 */
int BLO_is_a_library(char *path, char *dir, char *group);

struct Main* BLO_library_append_begin(const struct bContext *C, BlendHandle** bh, char *dir);
void BLO_library_append_named_part(const struct bContext *C, struct Main *mainl, BlendHandle** bh, char *name, int idcode, short flag);
void BLO_library_append_end(const struct bContext *C, struct Main *mainl, BlendHandle** bh, int idcode, short flag);

/* deprecated */
#if 1
void BLO_script_library_append(BlendHandle **bh, char *dir, char *name, int idcode, short flag, struct Main *mainvar, struct Scene *scene, struct ReportList *reports);
#endif

BlendFileData* blo_read_blendafterruntime(int file, char *name, int actualsize, struct ReportList *reports);

#ifdef __cplusplus
} 
#endif

#endif

