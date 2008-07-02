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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include "BLI_winstuff.h"
#include <io.h>
#include <direct.h>
#endif   
#include <fcntl.h>
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_imasel.h"
#include "BIF_filelist.h"
#include "BIF_space.h"
#include "BIF_screen.h"

#include "blendef.h"
#include "mydevice.h"


void free_imasel(SpaceImaSel *simasel)
{
	/* do not free imasel itself */
	if(simasel->files) {
		BIF_filelist_freelib(simasel->files);
		BIF_filelist_free(simasel->files);
		MEM_freeN(simasel->files);
		simasel->files = NULL;
	}
	if (simasel->img) {
		IMB_freeImBuf(simasel->img);
	}
	if(simasel->pupmenu) {
		MEM_freeN(simasel->pupmenu);
		simasel->pupmenu = NULL;		
	}
}

/* resets a previous imagebrowser space type */
/* is used when opening an imagebrowser directly from windowtype_pupmenu,
   since in that case we don't want any load/save/append/link action
*/
void reset_imaselspace(ScrArea *sa) 
{
	if(sa->spacetype==SPACE_IMASEL) {
		SpaceImaSel *simasel= sa->spacedata.first;
		if(simasel->type==FILE_MAIN) {
			if (simasel->files) {
				BIF_filelist_free(simasel->files);
				BIF_filelist_settype(simasel->files, FILE_MAIN);
			}
		} else {
			if (simasel->files) {
				simasel->type= FILE_UNIX;
				BIF_filelist_settype(simasel->files, simasel->type);
			}
		}
		simasel->returnfunc= NULL;
		simasel->title[0]= 0;
	}
}
