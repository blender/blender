/*
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
 * external readfile function prototypes
 */
#ifndef BLO_READFILE_H
#define BLO_READFILE_H

#ifdef __cplusplus
extern "C" {
#endif

struct SpaceFile;
struct SpaceImaSel;
struct FileList;
struct LinkNode;
struct Main;
struct UserDef;
struct bScreen;
struct Scene;
struct MemFile;
struct direntry;

typedef struct BlendHandle	BlendHandle;

typedef enum BlenFileType {
	BLENFILETYPE_BLEND= 1, 
	BLENFILETYPE_PUB= 2, 
	BLENFILETYPE_RUNTIME= 3
} BlenFileType;

typedef enum {
	BRE_NONE, 
	
	BRE_UNABLE_TO_OPEN, 
	BRE_UNABLE_TO_READ, 

	BRE_OUT_OF_MEMORY, 
	BRE_INTERNAL_ERROR, 

	BRE_NOT_A_BLEND, 
	BRE_NOT_A_PUBFILE,
	BRE_INCOMPLETE, 
	BRE_CORRUPT, 
	
	BRE_TOO_NEW, 
	BRE_NOT_ALLOWED, 
	
	BRE_NO_SCREEN, 
	BRE_NO_SCENE, 
	
	BRE_INVALID
} BlendReadError;

typedef struct BlendFileData {
	struct Main*	main;
	struct UserDef*	user;

	int winpos;
	int fileflags;
	int displaymode;
	int globalf;

	struct bScreen*	curscreen;
	struct Scene*	curscene;
	
	BlenFileType	type;
} BlendFileData;

	/**
	 * Open a blender file from a pathname. The function
	 * returns NULL and sets the @a error_r argument if
	 * it cannot open the file.
	 * 
	 * @param file The path of the file to open.
	 * @param error_r If the return value is NULL, an error
	 * code indicating the cause of the failure.
	 * @return The data of the file.
	 */
BlendFileData*	BLO_read_from_file		(char *file, BlendReadError *error_r);

	/**
	 * Open a blender file from memory. The function
	 * returns NULL and sets the @a error_r argument if
	 * it cannot open the file.
	 * 
	 * @param mem The file data.
	 * @param memsize The length of @a mem.
	 * @param error_r If the return value is NULL, an error
	 * code indicating the cause of the failure.
	 * @return The data of the file.
	 */
BlendFileData*	BLO_read_from_memory(void *mem, int memsize, BlendReadError *error_r);

/**
 * file name is current file, only for retrieving library data */

BlendFileData *BLO_read_from_memfile(const char *filename, struct MemFile *memfile, BlendReadError *error_r);

/**
 * Convert a BlendReadError to a human readable string.
 * The string is static and does not need to be free'd.
 * 
 * @param error The error to return a string for.
 * @return A static human readable string representation
 * of @a error.
 */
 
	char*
BLO_bre_as_string(
	BlendReadError error);

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

char *BLO_gethome(void);
int BLO_has_bfile_extension(char *str);
void BLO_library_append(struct SpaceFile *sfile, char *dir, int idcode);
void BLO_library_append_(BlendHandle **libfiledata, struct direntry* filelist, int totfile, char *dir, char* file, short flag, int idcode);
void BLO_script_library_append(BlendHandle **bh, char *dir, char *name, int idcode, short flag, struct Scene *scene);

BlendFileData* blo_read_blendafterruntime(int file, int actualsize, BlendReadError *error_r);

#ifdef __cplusplus
} 
#endif

#endif

