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
 * openssl/crypt RSA signature wrapper library
 */

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "openssl/rsa.h"
#include "openssl/ripemd.h"
#include "openssl/objects.h"
#include "zlib.h"

#include "GEN_messaging.h"

#include "BLO_keyStore.h"
#include "BLO_writeStreamGlue.h"
#include "BLO_sign_verify_Header.h"
#include "BLO_sign.h"

	int
BLO_sign(
	unsigned char *data2,
	unsigned int dataIn,
	struct streamGlueHeaderStruct *streamGlueHeader)
{
	int err = 0;
	struct writeStreamGlueStruct *streamGlue = NULL;
	struct BLO_sign_verify_HeaderStruct BLO_sign_verify_Header;
	struct BLO_SignerHeaderStruct BLO_SignerHeader;
	unsigned char *sigret = NULL;
	unsigned int siglen = 0;
	unsigned char *digest = NULL;
	byte *pubKey = NULL, *privKey = NULL;
	int pubKeyLen = 0, privKeyLen = 0;
	RSA *rsa = NULL;
	static unsigned char rsa_e[] = "\x01\x00\x01";
	int signSuccess = 0;
	unsigned char *data = NULL;

	// Update streamGlueHeader that initiated us and write it away
	streamGlueHeader->totalStreamLength =
		htonl(SIGNVERIFYHEADERSTRUCTSIZE + SIGNERHEADERSTRUCTSIZE + dataIn);
	streamGlueHeader->crc = htonl(crc32(0L,
		(const Bytef *) streamGlueHeader, STREAMGLUEHEADERSIZE - 4));
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *) streamGlueHeader,
		STREAMGLUEHEADERSIZE,
		0);
	if (err) return err;
	
#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_sign writes streamGlueHeader of %u bytes\n",
			STREAMGLUEHEADERSIZE);
#endif

	pubKeyLen = keyStoreGetPubKey(&pubKey);
	privKeyLen = keyStoreGetPrivKey(&privKey);
	if ((pubKeyLen == 0) || (privKeyLen == 0)) {
		err = BWS_SETFUNCTION(BWS_SIGN) |
			  BWS_SETGENERR(BWS_RSA);
		return err;
	}

	rsa = RSA_new();
	if (rsa == NULL) {
#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"Error in RSA_new\n");
#endif
		err = BWS_SETFUNCTION(BWS_SIGN) |
			  BWS_SETSPECERR(BWS_RSANEWERROR);
		return err;
	}
	// static exponent
	rsa->e = BN_bin2bn(rsa_e, sizeof(rsa_e)-1, rsa->e);

	// public part into rsa->n
	rsa->n = BN_bin2bn(pubKey, pubKeyLen, rsa->n);
	//rsa->n = BN_bin2bn(rsa_n, sizeof(rsa_n)-1, rsa->n);

	// private part into rsa->d
	rsa->d = BN_bin2bn(privKey, privKeyLen, rsa->d);
	//rsa->d = BN_bin2bn(rsa_d, sizeof(rsa_d)-1, rsa->d);

	//DEBUG RSA_print_fp(stdout, rsa, 0);

	sigret = malloc(RSA_size(rsa) * sizeof(byte));
	if (!sigret) {
		err = BWS_SETFUNCTION(BWS_SIGN) |
			  BWS_SETGENERR(BWS_MALLOC);
		RSA_free(rsa);
		return err;
	}

	digest = malloc(RIPEMD160_DIGEST_LENGTH);
	if (!digest) {
		err = BWS_SETFUNCTION(BWS_SIGN) |
			  BWS_SETGENERR(BWS_MALLOC);
		free(sigret);
		RSA_free(rsa);
		return err;
	}

	// Fill BLO_SignerHeader
	strcpy(BLO_SignerHeader.name, keyStoreGetUserName());
	strcpy(BLO_SignerHeader.email, keyStoreGetEmail());
	BLO_SignerHeader.homeUrl[0] = '\0';
	BLO_SignerHeader.text[0] = '\0';
	BLO_SignerHeader.pubKeyUrl1[0] = '\0';
	BLO_SignerHeader.pubKeyUrl2[0] = '\0';

	// prepend BLO_SignerStruct to data
	data = malloc(SIGNERHEADERSTRUCTSIZE + dataIn);
	if (!data) {
		err = BWS_SETFUNCTION(BWS_SIGN) |
			  BWS_SETGENERR(BWS_MALLOC);
		free(sigret);
		free(digest);
		RSA_free(rsa);
		return err;
	}
	memcpy(data, &BLO_SignerHeader, SIGNERHEADERSTRUCTSIZE);
	memcpy(data + SIGNERHEADERSTRUCTSIZE, data2, dataIn);
	dataIn += SIGNERHEADERSTRUCTSIZE;

	RIPEMD160(data, dataIn, digest);

	signSuccess =  RSA_sign(NID_ripemd160, digest,
							RIPEMD160_DIGEST_LENGTH, sigret, &siglen, rsa);
	if (signSuccess != 1) {
		err = BWS_SETFUNCTION(BWS_SIGN) |
			  BWS_SETSPECERR(BWS_SIGNERROR);
		free(data);
		free(sigret);
		free(digest);
		RSA_free(rsa);
		return err;
	}

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_sign writes BLO_sign_verify_Header of %u bytes\n",
			SIGNVERIFYHEADERSTRUCTSIZE);
#endif
	// write out our header
	BLO_sign_verify_Header.magic = 'A';
	BLO_sign_verify_Header.length = htonl(dataIn);
	memcpy(BLO_sign_verify_Header.pubKey, pubKey, pubKeyLen);
	BLO_sign_verify_Header.pubKeyLen = htonl(pubKeyLen);
	memcpy(BLO_sign_verify_Header.signature, sigret, siglen);
	BLO_sign_verify_Header.signatureLen = htonl(siglen);
	BLO_sign_verify_Header.datacrc = htonl(crc32(0L,
		(const Bytef *) data, dataIn));
	BLO_sign_verify_Header.headercrc = htonl(crc32(0L,
		(const Bytef *) &BLO_sign_verify_Header, SIGNVERIFYHEADERSTRUCTSIZE-4));
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *) &BLO_sign_verify_Header,
		SIGNVERIFYHEADERSTRUCTSIZE,
		0);
	if (err) {
		free(data);
		free(sigret);
		free(digest);
		RSA_free(rsa);
		return err;
	}

#ifndef NDEBUG
	fprintf(GEN_errorstream,
		"BLO_sign writes %u bytes raw data (plus its 2 headers totals to %u)\n",
		dataIn, STREAMGLUEHEADERSIZE + SIGNVERIFYHEADERSTRUCTSIZE + dataIn);
#endif
	// finally write all signed data
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *) data,
		dataIn,
		1);

	free(data); // NOTE: we must release because we made a copy
	free(digest);
	free(sigret);
	RSA_free(rsa);

	return err;
}

