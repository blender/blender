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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * openssl verify wrapper library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openssl/rsa.h"
#include "openssl/ripemd.h"
#include "openssl/objects.h"
#include "zlib.h"

#include "GEN_messaging.h"

#include "BLO_readStreamGlue.h"
#include "BLO_verify.h"
#include "BLO_sign_verify_Header.h"	/* used by verify and encrypt */

#include "BLO_signer_info.h" /* external signer info struct */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static struct BLO_SignerInfo g_SignerInfo = {"", "", ""};

struct verifyStructType {
	struct readStreamGlueStruct *streamGlue;
	unsigned int streamDone;
	unsigned char headerbuffer[SIGNVERIFYHEADERSTRUCTSIZE];
	uint32_t datacrc;
	struct BLO_sign_verify_HeaderStruct *streamHeader;
	RIPEMD160_CTX ripemd160_ctx;
	struct BLO_SignerHeaderStruct *signerHeader;
	unsigned char signerHeaderBuffer[SIGNERHEADERSTRUCTSIZE];
	void *endControl;
};

	BLO_verifyStructHandle
BLO_verify_begin(
	void *endControl)
{
	struct verifyStructType *control;
	control = malloc(sizeof(struct verifyStructType));
	if (!control) return NULL;

	control->streamGlue = NULL;
	control->streamDone = 0;
	memset(control->headerbuffer, 0, SIGNVERIFYHEADERSTRUCTSIZE);
	control->datacrc = 0;

	control->streamHeader = malloc(SIGNVERIFYHEADERSTRUCTSIZE);
	if (!control->streamHeader) {
		free(control);
		return NULL;
	}
	control->streamHeader->magic = 0;
	control->streamHeader->length = 0;
	strcpy(control->streamHeader->pubKey, "");
	control->streamHeader->pubKeyLen = 0;
	strcpy(control->streamHeader->signature, "");
	control->streamHeader->signatureLen = 0;
	control->streamHeader->datacrc = 0;
	control->streamHeader->headercrc = 0;

	RIPEMD160_Init(&(control->ripemd160_ctx));

	control->signerHeader = malloc(SIGNERHEADERSTRUCTSIZE);
	if (!control->signerHeader) {
		free(control->streamHeader);
		free(control);
		return NULL;
	}
	memset(control->signerHeader->name,       0, MAXSIGNERLEN);
	memset(control->signerHeader->email,      0, MAXSIGNERLEN);
	memset(control->signerHeader->homeUrl,    0, MAXSIGNERLEN);
	memset(control->signerHeader->text,       0, MAXSIGNERLEN);
	memset(control->signerHeader->pubKeyUrl1, 0, MAXSIGNERLEN);
	memset(control->signerHeader->pubKeyUrl2, 0, MAXSIGNERLEN);

	control->endControl = endControl;
	return((BLO_verifyStructHandle) control);
}

	int
BLO_verify_process(
	BLO_verifyStructHandle BLO_verifyHandle,
	unsigned char *data,
	unsigned int dataIn)
{
	int err = 0;
	struct verifyStructType *BLO_verify =
		(struct verifyStructType *) BLO_verifyHandle;

	if (!BLO_verify) {
		err = BRS_SETFUNCTION(BRS_VERIFY) |
			  BRS_SETGENERR(BRS_NULL);
		return err;
	}

	/* First check if we have our header filled in yet */
	if (BLO_verify->streamHeader->length == 0) {
		unsigned int processed;
		if (dataIn == 0) return err;	/* really need data to do anything */
		processed = ((dataIn + BLO_verify->streamDone) <=
					 SIGNVERIFYHEADERSTRUCTSIZE)
					 ? dataIn : SIGNVERIFYHEADERSTRUCTSIZE;
		memcpy(BLO_verify->headerbuffer + BLO_verify->streamDone,
			   data, processed);
		BLO_verify->streamDone += processed;
		dataIn -= processed;
		data += processed;
		if (BLO_verify->streamDone == SIGNVERIFYHEADERSTRUCTSIZE) {
			/* we have the whole header, absorb it */
			struct BLO_sign_verify_HeaderStruct *header;
			uint32_t crc;

			header = (struct BLO_sign_verify_HeaderStruct *)
				BLO_verify->headerbuffer;
			crc = crc32(0L, (const Bytef *) header,
						SIGNVERIFYHEADERSTRUCTSIZE - 4);

			if (header->magic == 'A') {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"BLO_sign_verify_HeaderStruct Magic confirmed\n");
#endif
			} else {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"ERROR BLO_sign_verify_HeaderStruct Magic NOT confirmed\n");
#endif
				err = BRS_SETFUNCTION(BRS_VERIFY) |
					  BRS_SETGENERR(BRS_MAGIC);
				if (BLO_verify->streamGlue) free(BLO_verify->streamGlue);
				if (BLO_verify->streamHeader) free(BLO_verify->streamHeader);
				if (BLO_verify->signerHeader) free(BLO_verify->signerHeader);
				free(BLO_verify);
				return err;
			}

			if (crc == ntohl(header->headercrc)) {
#ifndef NDEBUG				
				fprintf(GEN_errorstream,"BLO_sign_verify_Header CRC correct\n");
#endif
			} else {
#ifndef NDEBUG
				fprintf(GEN_errorstream,"ERROR BLO_sign_verify_Header CRC NOT correct\n");
#endif
				err = BRS_SETFUNCTION(BRS_VERIFY) |
					  BRS_SETGENERR(BRS_CRCHEADER);
				if (BLO_verify->streamGlue) free(BLO_verify->streamGlue);
				if (BLO_verify->streamHeader) free(BLO_verify->streamHeader);
				if (BLO_verify->signerHeader) free(BLO_verify->signerHeader);
				free(BLO_verify);
				return err;
			}
			BLO_verify->streamHeader->length = ntohl(header->length);
			BLO_verify->streamHeader->pubKeyLen = ntohl(header->pubKeyLen);
			memcpy(BLO_verify->streamHeader->pubKey, header->pubKey,
				   BLO_verify->streamHeader->pubKeyLen);
			BLO_verify->streamHeader->signatureLen =
				ntohl(header->signatureLen);
			memcpy(BLO_verify->streamHeader->signature, header->signature,
				   BLO_verify->streamHeader->signatureLen);
			BLO_verify->streamHeader->datacrc = ntohl(header->datacrc);

#ifndef NDEBUG
			fprintf(GEN_errorstream,
					"BLO_verify_process gets %u bytes\n",
					(unsigned int) BLO_verify->streamHeader->length);
#endif

		}
	}

	/* Is there really (still) new data available ? */
	if (dataIn > 0) {
		/* BLO_SignerHeaderStruct */
		if (BLO_verify->signerHeader->name[0] == 0) {
			/* we don't have our signerHeader complete yet */
			unsigned int processed;
			processed = ((dataIn + BLO_verify->streamDone -
				SIGNVERIFYHEADERSTRUCTSIZE) <= SIGNERHEADERSTRUCTSIZE)
				? dataIn : SIGNERHEADERSTRUCTSIZE;
			memcpy(BLO_verify->signerHeaderBuffer +
				   BLO_verify->streamDone - SIGNVERIFYHEADERSTRUCTSIZE,
				   data, processed);
			BLO_verify->streamDone += processed;
			dataIn -= processed;
			data += processed;
			if (BLO_verify->streamDone == SIGNVERIFYHEADERSTRUCTSIZE +
				SIGNERHEADERSTRUCTSIZE) {
				/* we have the whole header, absorb it */
				struct BLO_SignerHeaderStruct *signerHeader;
				signerHeader = (struct BLO_SignerHeaderStruct *)
					BLO_verify->signerHeaderBuffer;
				strncpy(BLO_verify->signerHeader->name,
						signerHeader->name, MAXSIGNERLEN-1);
				strncpy(BLO_verify->signerHeader->email,
						signerHeader->email, MAXSIGNERLEN-1);
				strncpy(BLO_verify->signerHeader->homeUrl,
					   signerHeader->homeUrl, MAXSIGNERLEN-1);
				strncpy(BLO_verify->signerHeader->text,
						signerHeader->text, MAXSIGNERLEN-1);
				strncpy(BLO_verify->signerHeader->pubKeyUrl1,
					   signerHeader->pubKeyUrl1, MAXSIGNERLEN-1);
				strncpy(BLO_verify->signerHeader->pubKeyUrl2,
					   signerHeader->pubKeyUrl2, MAXSIGNERLEN-1);

#ifndef NDEBUG
				fprintf(GEN_errorstream,
					"name %s\nemail %s\nhomeUrl %s\ntext %s\n",
					BLO_verify->signerHeader->name,
					BLO_verify->signerHeader->email,
					BLO_verify->signerHeader->homeUrl,
					BLO_verify->signerHeader->text);
				fprintf(GEN_errorstream,
					"pubKeyUrl1 %s\npubKeyUrl2 %s\n",
					BLO_verify->signerHeader->pubKeyUrl1,
					BLO_verify->signerHeader->pubKeyUrl2);
#endif
				/* also update the signature and crc checksum */
				RIPEMD160_Update(&(BLO_verify->ripemd160_ctx),
								 BLO_verify->signerHeaderBuffer,
								 SIGNERHEADERSTRUCTSIZE);

				/* update datacrc */
				BLO_verify->datacrc = crc32(
					BLO_verify->datacrc, (const Bytef *)
					BLO_verify->signerHeaderBuffer,
					SIGNERHEADERSTRUCTSIZE);
			}
		}
	}

	/* Is there really (still) new data available ? */
	if (dataIn > 0) {
		RIPEMD160_Update(&(BLO_verify->ripemd160_ctx), data, dataIn);

		/* update datacrc */
		BLO_verify->datacrc = crc32(
			BLO_verify->datacrc, (const Bytef *) data, dataIn);

		BLO_verify->streamDone += dataIn;

		/* give data to streamGlueRead, it will find out what to do next */
		err = readStreamGlue(
			BLO_verify->endControl,
			&(BLO_verify->streamGlue),
			(unsigned char *) data,
			dataIn);
	}
	return err;
}

/**
 * openssl verify final call and cleanup
 * @param BLO_verify Pointer to verify control structure
 * @retval streamGlueRead return value
 */
	int
BLO_verify_end(
	BLO_verifyStructHandle BLO_verifyHandle)
{
	int err = 0;
	unsigned char *digest;
	static unsigned char rsa_e[] = "\x01\x00\x01";
	RSA *rsa = NULL;
	int verifySuccess;
	struct verifyStructType *BLO_verify =
		(struct verifyStructType *) BLO_verifyHandle;

	if (!BLO_verify) {
		err = BRS_SETFUNCTION(BRS_VERIFY) |
			  BRS_SETGENERR(BRS_NULL);
		return err;
	}

	if (BLO_verify->streamDone == BLO_verify->streamHeader->length +
		SIGNVERIFYHEADERSTRUCTSIZE) {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Signed data length is correct\n");
#endif
	} else {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Signed data length is NOT correct\n");
#endif
		err = BRS_SETFUNCTION(BRS_VERIFY) |
			  BRS_SETGENERR(BRS_DATALEN);
		if (BLO_verify->streamGlue) free(BLO_verify->streamGlue);
		if (BLO_verify->streamHeader) free(BLO_verify->streamHeader);
		if (BLO_verify->signerHeader) free(BLO_verify->signerHeader);
		free(BLO_verify);
		return err;
	}

	if (BLO_verify->datacrc == BLO_verify->streamHeader->datacrc) {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Signed data CRC is correct\n");
#endif
	} else {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Signed data CRC is NOT correct\n");
#endif
		err = BRS_SETFUNCTION(BRS_VERIFY) |
			  BRS_SETGENERR(BRS_CRCDATA);
		if (BLO_verify->streamGlue) free(BLO_verify->streamGlue);
		if (BLO_verify->streamHeader) free(BLO_verify->streamHeader);
		if (BLO_verify->signerHeader) free(BLO_verify->signerHeader);
		free(BLO_verify);
		return err;
	}

	digest = malloc(RIPEMD160_DIGEST_LENGTH);
	if (!digest) {
		err = BRS_SETFUNCTION(BRS_VERIFY) |
			  BRS_SETGENERR(BRS_MALLOC);
		if (BLO_verify->streamGlue) free(BLO_verify->streamGlue);
		if (BLO_verify->streamHeader) free(BLO_verify->streamHeader);
		if (BLO_verify->signerHeader) free(BLO_verify->signerHeader);
		free(BLO_verify);
		return err;
	}

	RIPEMD160_Final(digest, &(BLO_verify->ripemd160_ctx));

	rsa = RSA_new();
	if (rsa == NULL) {
#ifndef NDEBUG
		fprintf(GEN_errorstream, "Error in RSA_new\n");
#endif
		err = BRS_SETFUNCTION(BRS_VERIFY) |
			  BRS_SETSPECERR(BRS_RSANEWERROR);
		free(digest);
		if (BLO_verify->streamGlue) free(BLO_verify->streamGlue);
		if (BLO_verify->streamHeader) free(BLO_verify->streamHeader);
		if (BLO_verify->signerHeader) free(BLO_verify->signerHeader);
		free(BLO_verify);
		return err;
	}
	/* static exponent */
	rsa->e = BN_bin2bn(rsa_e, sizeof(rsa_e)-1, rsa->e);

	/* public part into rsa->n */
	rsa->n = BN_bin2bn(BLO_verify->streamHeader->pubKey,
					   BLO_verify->streamHeader->pubKeyLen,
					   rsa->n);
	/*DEBUG RSA_print_fp(stdout, rsa, 0); */

	/* verify the signature */
	verifySuccess = RSA_verify(NID_ripemd160, digest, RIPEMD160_DIGEST_LENGTH,
				   BLO_verify->streamHeader->signature,
				   BLO_verify->streamHeader->signatureLen, rsa);
	if (verifySuccess == 1) {
#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"Signature verified\n");
#endif
	} else {
#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"Signature INCORRECT\n");
#endif
		err = BRS_SETFUNCTION(BRS_VERIFY) |
			  BRS_SETSPECERR(BRS_SIGFAILED);
	}

/* copy signer information to external struct  */
	
	strncpy(g_SignerInfo.name, BLO_verify->signerHeader->name, MAXSIGNERLEN-1);
	strncpy(g_SignerInfo.email, BLO_verify->signerHeader->email, MAXSIGNERLEN-1);
	strncpy(g_SignerInfo.homeUrl, BLO_verify->signerHeader->homeUrl, MAXSIGNERLEN-1);

	free(digest);
	free(BLO_verify->streamGlue);
	free(BLO_verify->streamHeader);
	free(BLO_verify->signerHeader);
	free(BLO_verify);
	RSA_free(rsa);

	return err;
}

struct BLO_SignerInfo *BLO_getSignerInfo(){
	return &g_SignerInfo;
}

int BLO_isValidSignerInfo(struct BLO_SignerInfo *info){
	return info->name[0] != 0;
}

void BLO_clrSignerInfo(struct BLO_SignerInfo *info)
{
	info->name[0] = 0;
}

