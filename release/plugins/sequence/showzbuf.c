/**
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
 */

#include <stdio.h>
#include "plugin.h"

/* ******************** GLOBAL VARIABLES ***************** */


char name[24]= "showzbuf";

/* structure for buttons, 
 *  butcode      name           default  min  max  0
 */

VarStruct varstr[]= {
	{NUMSLI|FLO,	"width ",		1.0,	0.0, 10.0, "This button is obsolete!"}
};

/* The cast struct is for input in the main doit function
   Varstr and Cast must have the same variables in the same order */ 

typedef struct Cast {
	float width;
} Cast;

/* cfra: the current frame */

float cfra;

void plugin_seq_doit(Cast *, float, float, int, int, ImBuf *, ImBuf *, ImBuf *, ImBuf *);

/* ******************** Fixed functions ***************** */

int plugin_seq_getversion(void) 
{
	return B_PLUGIN_VERSION;
}

void plugin_but_changed(int but) 
{
}

void plugin_init()
{
}

void plugin_getinfo(PluginInfo *info)
{
	info->name= name;
	info->nvars= sizeof(varstr)/sizeof(VarStruct);
	info->cfra= &cfra;

	info->varstr= varstr;

	info->init= plugin_init;
	info->seq_doit= (SeqDoit) plugin_seq_doit;
	info->callback= plugin_but_changed;
}

/* ************************************************************
	Show Zbuffer
	
	Demonstration of usage of the 32 bits zbuffer input.
	remember: z-values are not linear...
	
	Z values are only displayed when the input is a Scene-strip
	or when images were saved in IRIZ format.
	
   ************************************************************ */


void plugin_seq_doit(Cast *cast, float facf0, float facf1, int sx, int sy, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *out, ImBuf *use)
{
	int a;
	int *rectz;	
	char *rectc;
	
	if(ibuf1) {
		if(ibuf1->zbuf==0) {
			printf("no zbuf\n");
			return;
		}
		
		a= ibuf1->x*ibuf1->y;
		rectz= ibuf1->zbuf;
		rectc= (char *)out->rect;
		
		while(a--) {
			rectc[0]= 255;
			rectc[1]= rectc[2]= rectc[3]= (rectz[0]>>18);
			rectc+= 4;
			rectz++;
		}
	}

}

