/*
 * $Id: phix_manager.c 897 2011-08-28 21:43:57Z Kaori.Hagihara@gmail.com $
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
#include <math.h>
#include "opj_includes.h"

/* 
 * Write faix box of phix
 *
 * @param[in] coff      offset of j2k codestream
 * @param[in] compno    component number
 * @param[in] cstr_info codestream information
 * @param[in] EPHused   true if if EPH option used
 * @param[in] j2klen    length of j2k codestream
 * @param[in] cio       file output handle
 * @return              length of faix box
 */
int write_phixfaix( int coff, int compno, opj_codestream_info_t cstr_info, opj_bool EPHused, int j2klen, opj_cio_t *cio);

int write_phix( int coff, opj_codestream_info_t cstr_info, opj_bool EPHused, int j2klen, opj_cio_t *cio)
{
  int len, lenp=0, compno, i;
  opj_jp2_box_t *box;

  box = (opj_jp2_box_t *)opj_calloc( cstr_info.numcomps, sizeof(opj_jp2_box_t));
  
  for( i=0;i<2;i++){
    if (i) cio_seek( cio, lenp);
      
    lenp = cio_tell( cio);
    cio_skip( cio, 4);              /* L [at the end] */
    cio_write( cio, JPIP_PHIX, 4);  /* PHIX           */
      
    write_manf( i, cstr_info.numcomps, box, cio);

    for( compno=0; compno<cstr_info.numcomps; compno++){       
      box[compno].length = write_phixfaix( coff, compno, cstr_info, EPHused, j2klen, cio);
      box[compno].type = JPIP_FAIX;
    }

    len = cio_tell( cio)-lenp;
    cio_seek( cio, lenp);
    cio_write( cio, len, 4);        /* L              */
    cio_seek( cio, lenp+len);
  }

  opj_free(box);

  return len;
}

int write_phixfaix( int coff, int compno, opj_codestream_info_t cstr_info, opj_bool EPHused, int j2klen, opj_cio_t *cio)
{
  int len, lenp, tileno, version, i, nmax, size_of_coding; /* 4 or 8 */
  opj_tile_info_t *tile_Idx;
  opj_packet_info_t packet;
  int resno, precno, layno, num_packet;
  int numOfres, numOfprec, numOflayers;
  packet.end_ph_pos = packet.start_pos = -1;
  (void)EPHused; /* unused ? */

  if( j2klen > pow( 2, 32)){
    size_of_coding =  8;
    version = 1;
  }
  else{
    size_of_coding = 4;
    version = 0;
  }

  lenp = cio_tell( cio);
  cio_skip( cio, 4);              /* L [at the end]      */
  cio_write( cio, JPIP_FAIX, 4);  /* FAIX                */ 
  cio_write( cio, version,1);     /* Version 0 = 4 bytes */

  nmax = 0;
  for( i=0; i<=cstr_info.numdecompos[compno]; i++)
    nmax += cstr_info.tile[0].ph[i] * cstr_info.tile[0].pw[i] * cstr_info.numlayers;
  
  cio_write( cio, nmax, size_of_coding); /* NMAX */
  cio_write( cio, cstr_info.tw*cstr_info.th, size_of_coding);      /* M    */
  
  for( tileno=0; tileno<cstr_info.tw*cstr_info.th; tileno++){
    tile_Idx = &cstr_info.tile[ tileno];
    
    num_packet = 0;
    numOfres = cstr_info.numdecompos[compno] + 1;

    for( resno=0; resno<numOfres ; resno++){
      numOfprec = tile_Idx->pw[resno]*tile_Idx->ph[resno];
      for( precno=0; precno<numOfprec; precno++){
	numOflayers = cstr_info.numlayers;
	for( layno=0; layno<numOflayers; layno++){
	  
	  switch ( cstr_info.prog){
	  case LRCP:
	    packet = tile_Idx->packet[ ((layno*numOfres+resno)*cstr_info.numcomps+compno)*numOfprec+precno];
	    break;
	  case RLCP:
	    packet = tile_Idx->packet[ ((resno*numOflayers+layno)*cstr_info.numcomps+compno)*numOfprec+precno];
	    break;
	  case RPCL:
	    packet = tile_Idx->packet[ ((resno*numOfprec+precno)*cstr_info.numcomps+compno)*numOflayers+layno];
	    break;
	  case PCRL:
	    packet = tile_Idx->packet[ ((precno*cstr_info.numcomps+compno)*numOfres+resno)*numOflayers + layno];
	    break;
	  case CPRL:
	    packet = tile_Idx->packet[ ((compno*numOfprec+precno)*numOfres+resno)*numOflayers + layno];
	    break;
	  default:
	    fprintf( stderr, "failed to ppix indexing\n");
	  }

	  cio_write( cio, packet.start_pos-coff, size_of_coding);                /* start position */
	  cio_write( cio, packet.end_ph_pos-packet.start_pos+1, size_of_coding); /* length         */
	  
	  num_packet++;
	}
      }
    }

    /* PADDING */
    while( num_packet < nmax){
      cio_write( cio, 0, size_of_coding); /* start position            */
      cio_write( cio, 0, size_of_coding); /* length                    */
      num_packet++;
    }
  }

  len = cio_tell( cio)-lenp;
  cio_seek( cio, lenp);
  cio_write( cio, len, 4);        /* L  */
  cio_seek( cio, lenp+len);

  return len;
}
