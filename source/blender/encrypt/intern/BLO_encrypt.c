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
 * openssl/crypt rc4 crypto wrapper library
 */

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "openssl/rc4.h"
#include "openssl/rand.h"
#include "openssl/rsa.h"
#include "zlib.h"

#include "GEN_messaging.h"

#include "BLO_keyStore.h"
#include "BLO_writeStreamGlue.h"
#include "BLO_en_de_cryptHeader.h"
#include "BLO_encrypt.h"

	int
BLO_encrypt(
	unsigned char *data,
	unsigned int dataIn,
	struct streamGlueHeaderStruct *streamGlueHeader)
{
	int err = 0;
	struct writeStreamGlueStruct *streamGlue = NULL;
	struct BLO_en_de_cryptHeaderStruct BLO_en_de_cryptHeader;
	RC4_KEY *rc4_key;
	unsigned char *cryptBuf = NULL;
	byte *pubKey, *privKey;
	int pubKeyLen, privKeyLen;
	int cryptKeyLen = 16;
	int cryptedKeyLen;
	byte cryptKey[16];	// FIXME cryptKeyLen];
	RSA *rsa = NULL;
	byte *cryptedKey;
	static unsigned char rsa_e[] = "\x01\x00\x01";

	cryptBuf = malloc(dataIn);
	if (!cryptBuf) {
		err = BWS_SETFUNCTION(BWS_ENCRYPT) |
			  BWS_SETGENERR(BWS_MALLOC);
		return err;
	}

	rc4_key = malloc(sizeof(RC4_KEY));
	if (!rc4_key) {
		err = BWS_SETFUNCTION(BWS_ENCRYPT) |
			  BWS_SETGENERR(BWS_MALLOC);
		free(cryptBuf);
		return err;
	}

#ifdef _WIN32
	RAND_screen();
#endif
	RAND_bytes(cryptKey, cryptKeyLen);

	RC4_set_key(rc4_key, cryptKeyLen, cryptKey);
	RC4(rc4_key, dataIn, data, cryptBuf);

	free(rc4_key);

	// Update streamGlueHeader that initiated us and write it away
	streamGlueHeader->totalStreamLength =
		htonl(EN_DE_CRYPTHEADERSTRUCTSIZE + dataIn);
	streamGlueHeader->crc = htonl(crc32(0L,
		(const Bytef *) streamGlueHeader, STREAMGLUEHEADERSIZE - 4));
#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_encrypt writes streamGlueHeader of %u bytes\n",
			STREAMGLUEHEADERSIZE);
#endif
 
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *)streamGlueHeader,
		STREAMGLUEHEADERSIZE,
		0);
	if (err) {
		free(cryptBuf);
		return err;
	}

	pubKeyLen = keyStoreGetPubKey(&pubKey);
	privKeyLen = keyStoreGetPrivKey(&privKey);
	if ((pubKeyLen == 0) || (privKeyLen == 0)) {
		err = BWS_SETFUNCTION(BWS_ENCRYPT) |
			  BWS_SETGENERR(BWS_RSA);
		free(cryptBuf);
		return err;
	}

	rsa = RSA_new();
	if (rsa == NULL) {
#ifndef NDEBUG		
		fprintf(GEN_errorstream,
				"Error in RSA_new\n");
#endif
		err = BWS_SETFUNCTION(BWS_ENCRYPT) |
			  BWS_SETSPECERR(BWS_RSANEWERROR);
		free(cryptBuf);
		return err;
	}
	// static exponent
	rsa->e = BN_bin2bn(rsa_e, sizeof(rsa_e)-1, rsa->e);

	// public part into rsa->n
	rsa->n = BN_bin2bn(pubKey, pubKeyLen, rsa->n);

	// private part into rsa->d
	rsa->d = BN_bin2bn(privKey, privKeyLen, rsa->d);

	//DEBUG RSA_print_fp(stdout, rsa, 0);

	cryptedKey = malloc(RSA_size(rsa) * sizeof(byte));
	if (!cryptedKey) {
		err = BWS_SETFUNCTION(BWS_ENCRYPT) |
			  BWS_SETGENERR(BWS_MALLOC);
		free(cryptBuf);
		RSA_free(rsa);
		return err;
	}

	// crypt the cryptkey
	cryptedKeyLen = RSA_private_encrypt(cryptKeyLen, cryptKey, cryptedKey,
										rsa, RSA_PKCS1_PADDING);
	if (cryptedKeyLen == -1) {
		// TODO real error handling
#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"Error in RSA_private_encrypt\n");
#endif
		err = BWS_SETFUNCTION(BWS_ENCRYPT) |
			  BWS_SETSPECERR(BWS_ENCRYPTERROR);
		free(cryptBuf);
		free(cryptedKey);
		RSA_free(rsa);
		return err;
	}

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_encrypt writes BLO_en_de_cryptHeader of %u bytes\n",
			EN_DE_CRYPTHEADERSTRUCTSIZE);
#endif

	// write out our header
	BLO_en_de_cryptHeader.magic = 'A';
	BLO_en_de_cryptHeader.length = htonl(dataIn);
	memcpy(BLO_en_de_cryptHeader.pubKey, pubKey, pubKeyLen);
	BLO_en_de_cryptHeader.pubKeyLen = htonl(pubKeyLen);
	memcpy(BLO_en_de_cryptHeader.cryptedKey, cryptedKey, cryptedKeyLen);
	BLO_en_de_cryptHeader.cryptedKeyLen = htonl(cryptedKeyLen);
	BLO_en_de_cryptHeader.datacrc = htonl(crc32(0L,
		(const Bytef *) cryptBuf, dataIn));
	BLO_en_de_cryptHeader.headercrc = htonl(crc32(0L,
		(const Bytef *) &BLO_en_de_cryptHeader, EN_DE_CRYPTHEADERSTRUCTSIZE-4));
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *) &BLO_en_de_cryptHeader,
		EN_DE_CRYPTHEADERSTRUCTSIZE,
		0);
	if (err) {
		free(cryptBuf);
		free(cryptedKey);
		RSA_free(rsa);
		return err;
	}

#ifndef NDEBUG 
	fprintf(GEN_errorstream,
			"BLO_encrypt writes %u bytes raw data (total %u)\n",
			dataIn,
			STREAMGLUEHEADERSIZE + EN_DE_CRYPTHEADERSTRUCTSIZE + dataIn);
#endif

	// finally write all crypted data
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *) cryptBuf,
		dataIn,
		1);

	free(cryptBuf);
	free(cryptedKey);
	RSA_free(rsa);

	return err;
}

