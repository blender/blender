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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef WIN32
#include <io.h>
#include <direct.h>
#include "BLI_winstuff.h"
#else
#include <unistd.h>
#include <sys/times.h>
#endif   

#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_storage_types.h"
#include "BLI_dynstr.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_fileselect.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "file_intern.h"
#include "filelist.h"


FileSelectParams* ED_fileselect_get_params(struct SpaceFile *sfile)
{
	if (!sfile->params) {
		ED_fileselect_set_params(sfile, FILE_UNIX, "", "/", 0, 0, 0);
	}
	return sfile->params;
}

short ED_fileselect_set_params(SpaceFile *sfile, int type, const char *title, const char *path,
							   short flag, short display, short filter)
{
	char name[FILE_MAX], dir[FILE_MAX], file[FILE_MAX];
	FileSelectParams *params;

	if (!sfile->params) {
		sfile->params= MEM_callocN(sizeof(FileSelectParams), "fileselparams");
	}

	params = sfile->params;

	params->type = type;
	params->flag = flag;
	params->display = display;
	params->filter = filter;

	BLI_strncpy(params->title, title, sizeof(params->title));
	
	BLI_strncpy(name, path, sizeof(name));
	BLI_convertstringcode(name, G.sce);
	
	switch(type) {
		case FILE_MAIN:
			break;
		case FILE_LOADLIB:
			break;
		case FILE_BLENDER:
		case FILE_LOADFONT:
		default:
			{
				BLI_split_dirfile(name, dir, file);
				BLI_strncpy(params->file, file, sizeof(params->file));
				BLI_strncpy(params->dir, dir, sizeof(params->dir));
				BLI_make_file_string(G.sce, params->dir, dir, ""); /* XXX needed ? - also solve G.sce */			
			}
			break;
	}

	return 1;
}

void ED_fileselect_reset_params(SpaceFile *sfile)
{
	sfile->params->type = FILE_UNIX;
	sfile->params->flag = 0;
	sfile->params->title[0] = '\0';
}
