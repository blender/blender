/** \file blender/editors/datafiles/bunifont.ttf.c
 *  \ingroup eddatafiles
 */
/* DataToC output of file <bfont_ttf> */

#include <zlib.h>
#include <stdio.h>
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_memarena.h"
#include "MEM_guardedalloc.h"

const int datatoc_bunifont_ttf_size = 16336376;
static char* datatoc_bunifont_ttf = 0;

static char unifont_path[1024];
static char unifont_filename[]="unifont-5.1.20080907.ttf.zip";

char *get_datatoc_bunifont_ttf(void)
{
    if( datatoc_bunifont_ttf==NULL )
    {
        char *fontpath = BLI_get_folder(BLENDER_DATAFILES, "fonts");
        BLI_snprintf( unifont_path, sizeof(unifont_path), "%s/%s", fontpath, unifont_filename );

        if( BLI_exists(unifont_path) )
        {
			datatoc_bunifont_ttf = (char*)MEM_mallocN( datatoc_bunifont_ttf_size, "get_datatoc_bunifont_ttf" );
			BLI_ungzip_to_mem( unifont_path, datatoc_bunifont_ttf, datatoc_bunifont_ttf_size );
        }
    }
    return datatoc_bunifont_ttf;
}

void free_datatoc_bunifont_ttf(void)
{
	if( datatoc_bunifont_ttf!=NULL )
		MEM_freeN( datatoc_bunifont_ttf );
}
