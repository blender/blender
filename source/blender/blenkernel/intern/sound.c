/**
 * sound.c (mar-2001 nzc)
 *	
 * $Id$
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_packedFile_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sound.h"
#include "BKE_packedFile.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

ListBase _samples = {0,0}, *samples = &_samples;

void sound_free_sound(bSound *sound)
{
	/* this crashes blender, PLEASE fix! */
	// still crashes :(
	//if (sound) {
	//	sound_set_sample(sound, NULL);
	//}
	if (sound->stream) MEM_freeN(sound->stream);
}

void sound_free_sample(bSample *sample)
{
	if (sample) {	
		if (sample->data != &sample->fakedata[0] && sample->data != NULL) {
			MEM_freeN(sample->data);
			sample->data = &sample->fakedata[0];
		}
		
		if (sample->packedfile) {
			freePackedFile(sample->packedfile);  //FIXME: crashes sometimes 
			sample->packedfile = NULL;
		}
		
		if (sample->alindex != SAMPLE_INVALID) {
//			AUD_free_sample(sample->snd_sample);
			sample->alindex = SAMPLE_INVALID;
		}

		sample->type = SAMPLE_INVALID;
	}
}



void sound_set_packedfile(bSample *sample, PackedFile *pf)
{
	bSound *sound;
	
	if (sample) {
		sample->packedfile = pf;
		sound = G.main->sound.first;
		while (sound) {
			if (sound->sample == sample) {
				sound->newpackedfile = pf;
				if (pf == NULL) {
					strcpy(sound->name, sample->name);
				}
			}
			sound = sound->id.next;
		}
	}
}

PackedFile* sound_find_packedfile(bSound *sound) 
{
	bSound *search;
	PackedFile *pf = NULL;
	char soundname[FILE_MAXDIR + FILE_MAXFILE], searchname[FILE_MAXDIR + FILE_MAXFILE];
	
	// convert sound->name to abolute filename
	strcpy(soundname, sound->name);
	BLI_convertstringcode(soundname, G.sce, G.scene->r.cfra);
	
	search = G.main->sound.first;
	while (search) {
		if (search->sample && search->sample->packedfile) {
			strcpy(searchname, search->sample->name);
			BLI_convertstringcode(searchname, G.sce, G.scene->r.cfra);
			
			if (BLI_streq(searchname, soundname)) {
				pf = search->sample->packedfile;
				break;
			}
		} 
		
		if (search->newpackedfile) {
			strcpy(searchname, search->name);
			BLI_convertstringcode(searchname, G.sce, G.scene->r.cfra);
			if (BLI_streq(searchname, soundname)) {
				pf = search->newpackedfile;
				break;
			}
		}
		search = search->id.next;
	}
	
	return (pf);
}
