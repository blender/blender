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
 * openssl decryption wrapper library
 */

#include <stdio.h>
#include <string.h> /* memcpy, strcpy */
#include <stdlib.h>

#include "openssl/rsa.h"
#include "openssl/rc4.h"
#include "openssl/err.h"
#include "zlib.h"

#include "GEN_messaging.h"

#include "BLO_getPubKey.h"	// real and stub implemented at writestream ...

#include "BLO_readStreamGlue.h"
#include "BLO_decrypt.h"
#include "BLO_en_de_cryptHeader.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct decryptStructType {
	struct readStreamGlueStruct *streamGlue;
	unsigned int streamDone;
	unsigned char *deCryptKey;
	int deCryptKeyLen;			// NOTE: signed int
	unsigned char headerbuffer[EN_DE_CRYPTHEADERSTRUCTSIZE];
	uint32_t datacrc;			// crypted data checksum
	struct BLO_en_de_cryptHeaderStruct *streamHeader;
	RC4_KEY rc4_key;
	void *endControl;
};

	BLO_decryptStructHandle
BLO_decrypt_begin(
	void *endControl)
{
	struct decryptStructType *control;
	control = malloc(sizeof(struct decryptStructType));
	if (!control) return NULL;

	control->streamGlue = NULL;
	control->streamDone = 0;
	control->deCryptKey = NULL;
	control->deCryptKeyLen = 0;
	strcpy(control->headerbuffer, "");
	control->datacrc = 0;

	control->streamHeader = malloc(EN_DE_CRYPTHEADERSTRUCTSIZE);
	if (!control->streamHeader) {
		free(control);
		return NULL;
	}

	control->streamHeader->magic = 0;
	control->streamHeader->length = 0;
	strcpy(control->streamHeader->pubKey, "");
	control->streamHeader->pubKeyLen = 0;
	strcpy(control->streamHeader->cryptedKey, "");
	control->streamHeader->cryptedKeyLen = 0;
	control->streamHeader->datacrc = 0;
	control->streamHeader->headercrc = 0;
	control->endControl = endControl;

	return((BLO_decryptStructHandle) control);
}

	int
BLO_decrypt_process(
	BLO_decryptStructHandle BLO_decryptHandle,
	unsigned char *data,
	unsigned int dataIn)
{
	int err = 0;
	struct decryptStructType *BLO_decrypt =
		(struct decryptStructType *) BLO_decryptHandle;

	if (!BLO_decrypt) {
		err = BRS_SETFUNCTION(BRS_DECRYPT) |
			  BRS_SETGENERR(BRS_NULL);
		return err;
	}

	/* First check if we have our header filled in yet */
	if (BLO_decrypt->streamHeader->cryptedKeyLen == 0) {
		unsigned int processed;
		if (dataIn == 0) return err;	/* really need data to do anything */
		processed = ((dataIn + BLO_decrypt->streamDone) <=
					 EN_DE_CRYPTHEADERSTRUCTSIZE)
					 ? dataIn : EN_DE_CRYPTHEADERSTRUCTSIZE;
		memcpy(BLO_decrypt->headerbuffer + BLO_decrypt->streamDone,
			   data, processed);
		BLO_decrypt->streamDone += processed;
		dataIn -= processed;
		data += processed;
		if (BLO_decrypt->streamDone == EN_DE_CRYPTHEADERSTRUCTSIZE) {
			/* we have the whole header, absorb it */
			struct BLO_en_de_cryptHeaderStruct *header;
			uint32_t crc;
			//static unsigned char rsa_e[] = "\x11";
			static unsigned char rsa_e[] = "\x01\x00\x01";
			RSA *rsa = NULL;
			unsigned char *publisherPubKey;
			int publisherPubKeyLen;

			header = (struct BLO_en_de_cryptHeaderStruct *)
				BLO_decrypt->headerbuffer;
			crc = crc32(0L, (const Bytef *) header,
						EN_DE_CRYPTHEADERSTRUCTSIZE - 4);

			if (header->magic == 'A') {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"BLO_en_de_cryptHeaderStruct Magic confirmed\n");
#endif
			} else {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"ERROR BLO_en_de_cryptHeaderStruct Magic NOT confirmed\n");
#endif
				err = BRS_SETFUNCTION(BRS_DECRYPT) |
					  BRS_SETGENERR(BRS_MAGIC);
				if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
				if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
				if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
				free(BLO_decrypt);
				return err;
			}

			if (crc == ntohl(header->headercrc)) {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"BLO_en_de_cryptHeader CRC correct\n");
#endif
			} else {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"ERROR BLO_en_de_cryptHeader CRC NOT correct\n");
#endif
				err = BRS_SETFUNCTION(BRS_DECRYPT) |
					  BRS_SETGENERR(BRS_CRCHEADER);
				if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
				if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
				if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
				free(BLO_decrypt);
				return err;
			}
			BLO_decrypt->streamHeader->length = ntohl(header->length);
			BLO_decrypt->streamHeader->pubKeyLen = ntohl(header->pubKeyLen);
			memcpy(BLO_decrypt->streamHeader->pubKey, header->pubKey,
				   BLO_decrypt->streamHeader->pubKeyLen);

			// case Publisher: get the .BPkey public key
			// case Player/Plugin: simply use the data stream public key
			err = getPubKey(BLO_decrypt->streamHeader->pubKey,
							BLO_decrypt->streamHeader->pubKeyLen,
							&publisherPubKey,
							&publisherPubKeyLen);
			switch (err) {
				case 0:
					// everything OK
					break;
				case 1:
					// publisher without a key
				case 2:
					// publishers keylen !=
				case 3:
					// publishers key !=
				default:
#ifndef NDEBUG
					fprintf(GEN_errorstream,
						"ALERT users-pubKey != datastream-pubKey, stop reading\n");
#endif
					err = BRS_SETFUNCTION(BRS_DECRYPT) |
						  BRS_SETSPECERR(BRS_NOTOURPUBKEY);
					if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
					if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
					if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
					free(BLO_decrypt);
					return err;
					break;
			}

			BLO_decrypt->streamHeader->cryptedKeyLen =
				ntohl(header->cryptedKeyLen);
			memcpy(BLO_decrypt->streamHeader->cryptedKey,
				   header->cryptedKey,
				   BLO_decrypt->streamHeader->cryptedKeyLen);
#ifndef NDEBUG
			fprintf(GEN_errorstream,
					"BLO_decrypt_process gets %u bytes\n",
					(unsigned int) BLO_decrypt->streamHeader->length);
#endif
			BLO_decrypt->streamHeader->datacrc = ntohl(header->datacrc);

			// finished absorbing and testing the header, create rsa key from it
			rsa = RSA_new();
			if (rsa == NULL) {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"Error in RSA_new\n");
#endif
				err = BRS_SETFUNCTION(BRS_DECRYPT) |
					  BRS_SETSPECERR(BRS_RSANEWERROR);
				if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
				if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
				if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
				free(BLO_decrypt);
				return err;
			}
			// static exponent
			rsa->e = BN_bin2bn(rsa_e, sizeof(rsa_e)-1, rsa->e);

			// public part into rsa->n
			rsa->n = BN_bin2bn(publisherPubKey,
							   publisherPubKeyLen,
							   rsa->n);

			//DEBUG RSA_print_fp(stdout, rsa, 0);

			BLO_decrypt->deCryptKey = malloc(RSA_size(rsa) *
											 sizeof(unsigned char));
			if (! BLO_decrypt->deCryptKey) {
				err = BRS_SETFUNCTION(BRS_DECRYPT) |
					  BRS_SETGENERR(BRS_MALLOC);
				if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
				if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
				if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
				free(BLO_decrypt);
				RSA_free(rsa);
				return err;
			}

			// decrypt the cryptkey
			BLO_decrypt->deCryptKeyLen = RSA_public_decrypt(
				BLO_decrypt->streamHeader->cryptedKeyLen,
				BLO_decrypt->streamHeader->cryptedKey,
				BLO_decrypt->deCryptKey,
				rsa, RSA_PKCS1_PADDING);
			if (BLO_decrypt->deCryptKeyLen == -1) {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"Error in RSA_public_decrypt %s\n",
						ERR_error_string(ERR_get_error(),
										 NULL));
#endif
				err = BRS_SETFUNCTION(BRS_DECRYPT) |
					  BRS_SETSPECERR(BRS_DECRYPTERROR);
				if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
				if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
				if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
				free(BLO_decrypt);
				RSA_free(rsa);
				return err;
			}

			// Finally set the RC4 deCryptKey
			RC4_set_key(&(BLO_decrypt->rc4_key),
						BLO_decrypt->deCryptKeyLen,
						BLO_decrypt->deCryptKey);

			RSA_free(rsa);
		}
	}

	/* Is there really (still) new data available ? */
	if (dataIn > 0) {
		unsigned char *deCryptBuf = malloc(dataIn);
		if (! deCryptBuf) {
			err = BRS_SETFUNCTION(BRS_DECRYPT) |
				  BRS_SETGENERR(BRS_MALLOC);
			if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
			if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
			if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
			free(BLO_decrypt);
			return err;
		}

		BLO_decrypt->streamDone += dataIn;

		// update datacrc
		BLO_decrypt->datacrc = crc32(
			BLO_decrypt->datacrc, (const Bytef *) data, dataIn);

		// TODO FIXME we might need to keylength-align the data !
		RC4(&(BLO_decrypt->rc4_key), dataIn, data, deCryptBuf);

		// give data to streamGlueRead, it will find out what to do next
		err = readStreamGlue(
			BLO_decrypt->endControl,
			&(BLO_decrypt->streamGlue),
			(unsigned char *) deCryptBuf,
			dataIn);

		free(deCryptBuf);
	}
	return err;
}

/**
 * openssl decrypt final call and cleanup
 * @param BLO_decrypt Pointer to decrypt control structure
 * @retval streamGlueRead return value
 */
	int
BLO_decrypt_end(
	BLO_decryptStructHandle BLO_decryptHandle)
{
	int err = 0;
	struct decryptStructType *BLO_decrypt =
		(struct decryptStructType *) BLO_decryptHandle;

	if (!BLO_decrypt) {
		err = BRS_SETFUNCTION(BRS_DECRYPT) |
			  BRS_SETGENERR(BRS_NULL);
		if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
		if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
		if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
		free(BLO_decrypt);
		return err;
	}

	if (BLO_decrypt->streamDone == BLO_decrypt->streamHeader->length +
		EN_DE_CRYPTHEADERSTRUCTSIZE) {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Crypted data length is correct\n");
#endif
	} else {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Crypted data length is NOT correct\n");
#endif
		err = BRS_SETFUNCTION(BRS_DECRYPT) |
			  BRS_SETGENERR(BRS_DATALEN);
		if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
		if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
		if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
		free(BLO_decrypt);
		return err;
	}

	if (BLO_decrypt->datacrc == BLO_decrypt->streamHeader->datacrc) {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Crypted data CRC is correct\n");
#endif
	} else {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Crypted data CRC is NOT correct\n");
#endif
		err = BRS_SETFUNCTION(BRS_DECRYPT) |
			  BRS_SETGENERR(BRS_CRCDATA);
		if (BLO_decrypt->streamGlue) free(BLO_decrypt->streamGlue);
		if (BLO_decrypt->streamHeader) free(BLO_decrypt->streamHeader);
		if (BLO_decrypt->deCryptKey) free(BLO_decrypt->deCryptKey);
		free(BLO_decrypt);
		return err;
	}

	free(BLO_decrypt->streamGlue);
	free(BLO_decrypt->streamHeader);
	free(BLO_decrypt->deCryptKey);
	free(BLO_decrypt);

	return err;
}

