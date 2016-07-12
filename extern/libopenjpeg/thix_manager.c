/*
 * $Id: thix_manager.c 897 2011-08-28 21:43:57Z Kaori.Hagihara@gmail.com $
 *
 * Copyright (c) 2002-2011, Communications and Remote Sensing Laboratory, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2011, Professor Benoit Macq
 * Copyright (c) 2003-2004, Yannick Verschueren
 * Copyright (c) 2010-2011, Kaori Hagihara
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*! \file
 *  \brief Modification of jpip.c from 2KAN indexer
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "opj_includes.h"

/* 
 * Write tile-part headers mhix box
 *
 * @param[in] coff      offset of j2k codestream
 * @param[in] cstr_info codestream information
 * @param[in] tileno    tile number
 * @param[in] cio       file output handle
 * @return              length of mhix box
 */
int write_tilemhix( int coff, opj_codestream_info_t cstr_info, int tileno, opj_cio_t *cio);

int write_thix( int coff, opj_codestream_info_t cstr_info, opj_cio_t *cio)
{
  int len, lenp, i;
  int tileno;
  opj_jp2_box_t *box;

  lenp = 0;
  box = (opj_jp2_box_t *)opj_calloc( cstr_info.tw*cstr_info.th, sizeof(opj_jp2_box_t));

  for ( i = 0; i < 2 ; i++ ){
    if (i)
      cio_seek( cio, lenp);

    lenp = cio_tell( cio);
    cio_skip( cio, 4);              /* L [at the end] */
    cio_write( cio, JPIP_THIX, 4);  /* THIX           */
    write_manf( i, cstr_info.tw*cstr_info.th, box, cio);
    
    for (tileno = 0; tileno < cstr_info.tw*cstr_info.th; tileno++){
      box[tileno].length = write_tilemhix( coff, cstr_info, tileno, cio);
      box[tileno].type = JPIP_MHIX;
    }
 
    len = cio_tell( cio)-lenp;
    cio_seek( cio, lenp);
    cio_write( cio, len, 4);        /* L              */
    cio_seek( cio, lenp+len);
  }

  opj_free(box);

  return len;
}

int write_tilemhix( int coff, opj_codestream_info_t cstr_info, int tileno, opj_cio_t *cio)
{
  int i;
  opj_tile_info_t tile;
  opj_tp_info_t tp;
  int len, lenp;
  opj_marker_info_t *marker;

  lenp = cio_tell( cio);
  cio_skip( cio, 4);                               /* L [at the end]                    */
  cio_write( cio, JPIP_MHIX, 4);                   /* MHIX                              */

  tile = cstr_info.tile[tileno];
  tp = tile.tp[0];

  cio_write( cio, tp.tp_end_header-tp.tp_start_pos+1, 8);  /* TLEN                              */ 

  marker = cstr_info.tile[tileno].marker;

  for( i=0; i<cstr_info.tile[tileno].marknum; i++){             /* Marker restricted to 1 apparition */
    cio_write( cio, marker[i].type, 2);
    cio_write( cio, 0, 2);
    cio_write( cio, marker[i].pos-coff, 8);
    cio_write( cio, marker[i].len, 2);
  }
     
  /*  free( marker);*/

  len = cio_tell( cio) - lenp;
  cio_seek( cio, lenp);
  cio_write( cio, len, 4);        /* L           */
  cio_seek( cio, lenp+len);

  return len;
}
