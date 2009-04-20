/*  SND_test.c   nov 2000
*  
*  testfile for the SND module
* 
* janco verduin
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

#include "SND_C-api.h"
#include "BlenderWaveCacheCApi.h"
#include "OpenALC-Api.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>

static int buf[3];

float oPos[3]={3.0, 0.0,-1.0};
float oVel[3]={0.0, 0.0, 1.0};
float oOri[6]={0.0, 0.0, 1.0, 0.0, 1.0, 0.0};

void* ReadFile(char *filename)
{
	int file, filelen;
	void *data = NULL;

#if defined(WIN32)	
	file = open(filename, O_BINARY|O_RDONLY);
#else
	file = open(filename, 0|O_RDONLY);
#endif

	if (file == -1) {
		printf("can't open file.\n");
		printf("press q for quit.\n");

	}
	else {
		filelen = lseek(file, 0, SEEK_END);
		lseek(file, 0, SEEK_SET);
		
		if (filelen != 0){
			data = malloc(filelen);
			if (read(file, data, filelen) != filelen) {
				free(data);
				data = NULL;
			}
		}
		close(file);
		
	}
	return (data);
}

int main(int argc, char* argv[])
{
	int ch;
	char* samplename = NULL;
	void* sampleinmemory = NULL;
	SND_CacheHandle	wavecache = NULL;
	SND_SceneHandle	scene = NULL;
	SND_ObjectHandle object = NULL;
	
	wavecache = SND_GetWaveCache();
	scene = SND_CreateOpenALScene(wavecache);

	samplename = "2.wav";
	sampleinmemory = ReadFile(samplename);
	
	if (sampleinmemory) {

		object = SND_CreateObject();
		SND_AddMemoryLocation(samplename, sampleinmemory);
		SND_SetSampleName(object, samplename);
		SND_AddObject(scene, object);
		printf("go your gang...\n");
		printf("1: play\n");
		printf("2: stop\n");
		printf("q: quit\n");
	}
	do
	{
		ch = getchar();
		ch = toupper(ch);
		switch (ch)
		{
		case '1':
			{
				SND_SetPitch(object, 1.0);
				SND_SetGain(object, 1.0);
				SND_StartSound(object);
				break;
			}
		case '2':
			{
				SND_StopSound(object);
				break;
			}
		default:
			break;
		}

		SND_Proceed(scene);

	} while (ch != 'Q');
	
	if (object) {
		
		SND_RemoveObject(scene, object);
		SND_DeleteObject(object);
	}
	
	SND_DeleteScene(scene);
	SND_DeleteCache();
	
	return 0;
	
}
