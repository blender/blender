/**
 * $Id$
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

/* ex:ts=4 */

/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Blender Key Read-tester
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "key_pyc.h" /* the Python byte code */
#include "blenkey.h"	/* the external interface */
#include "key_internal.h"

#define TESTReadKeyFile 1

int Check_All_Byte_Calculus_Data(char *KeyBytePtr) {
	int i;

	/* create some unique number arrays */
	int NoRealRandomArray[MAXBYTEDATABLOCK];

	typedef byte (*funcpoin)(byte, byte);
	funcpoin checkfunc[] = {&checkfunc0, &checkfunc1, &checkfunc2,
		&checkfunc3, &checkfunc4};

	byte *KeyByteData = DeHexify(KeyBytePtr);

	/* first create a fixed seed random number generator */
	sgenrand(666); /* seed it fixed */

	/* initialize arrays with unique numbers */
	for (i=0; i<MAXBYTEDATABLOCK; i++) {
		NoRealRandomArray[i] = i;
	}
	/* then stir the unique number lists */
	for (i=0; i<MAXBYTEDATABLOCK; i++) {
		unsigned long randswap = genrand();
		int swap1 = (int) (randswap % MAXBYTEDATABLOCK);
		int swap2 = (int) ((randswap>>16) % MAXBYTEDATABLOCK);
		int store = NoRealRandomArray[swap1];
		/* printf("%lu %d %d\n", randswap, swap1, swap2); */
		NoRealRandomArray[swap1] = NoRealRandomArray[swap2];
		NoRealRandomArray[swap2] = store;
	}
	if (DEBUG) {
		printf("\nFixed seed unique number random data: ");
		for (i=0; i<MAXBYTEDATABLOCK; i++) {
			printf("%d ", NoRealRandomArray[i]);
		}
		printf("\n\n");
	}

	/* check our byte calculus functions on the random data */
	for (i=0; i<(MAXBYTEDATABLOCK-3); i+=3) {
		if (DEBUG) {
			char *Pcheckfunc[] = {"max", " - ", " + ", " * ", " / "};
			printf("[%3d]=[%3d]%s[%3d]    ",
				NoRealRandomArray[i], NoRealRandomArray[i+1],
				Pcheckfunc[NoRealRandomArray[i]%5], NoRealRandomArray[i+2]);
			printf("%3d%s%3d: %3d == %3d\n",
				KeyByteData[NoRealRandomArray[i+1]],
				Pcheckfunc[NoRealRandomArray[i]%5],
				KeyByteData[NoRealRandomArray[i+2]],
				KeyByteData[NoRealRandomArray[i]],
				checkfunc[(NoRealRandomArray[i]%5)](
					KeyByteData[NoRealRandomArray[i+1]],
					KeyByteData[NoRealRandomArray[i+2]]));
		}
		if (KeyByteData[NoRealRandomArray[i]] !=
			checkfunc[(NoRealRandomArray[i]%5)](
			KeyByteData[NoRealRandomArray[i+1]],
			KeyByteData[NoRealRandomArray[i+2]])) {
			printf("\nByte Checksum failed !\n");
			return (1);
		}
	}
	return (0);
}

void pub_priv_test(char *HexPriv, char *HexPub)
{
	RSA *rsa = NULL;
	/* static unsigned char rsa_e[] = "\x11"; */
	static unsigned char rsa_e[] = "\x01\x00\x01";
	byte cryptKey[16];
	byte *cryptedKey;
	byte *pubKey, *privKey;
	int pubKeyLen, privKeyLen;
	int deCryptKeyLen;
	unsigned char *deCryptKey;
	int cryptKeyLen = 16;
	int cryptedKeyLen;

	strcpy(cryptKey, "abcdefghijklmno");

	pubKey = DeHexify(HexPub);
	pubKeyLen = strlen(HexPub) / 2;

	privKey = DeHexify(HexPriv);
	privKeyLen = strlen(HexPriv) / 2;

	rsa = RSA_new();
	if (rsa == NULL) {
		fprintf(stderr, "Error in RSA_new\n");
		exit(1);
	}
	rsa->e = BN_bin2bn(rsa_e, sizeof(rsa_e)-1, rsa->e);
	rsa->n = BN_bin2bn(pubKey, pubKeyLen, rsa->n);
	rsa->d = BN_bin2bn(privKey, privKeyLen, rsa->d);

	fprintf(stderr, "NOTE e %d, n %d, d %d rsa_size %d\n",
			sizeof(rsa_e)-1, pubKeyLen, privKeyLen, RSA_size(rsa));

	cryptedKey = malloc(RSA_size(rsa) * sizeof(byte));
	cryptedKeyLen = RSA_private_encrypt(cryptKeyLen, cryptKey,
										cryptedKey, rsa, RSA_PKCS1_PADDING);

	deCryptKey = malloc(RSA_size(rsa) * sizeof(unsigned char));
	deCryptKeyLen = RSA_public_decrypt(cryptedKeyLen, cryptedKey,
									   deCryptKey, rsa, RSA_PKCS1_PADDING);
	if (deCryptKeyLen == -1) {
		printf("Error in RSA_public_decrypt: %s\n",
			   ERR_error_string(ERR_get_error(), NULL));
		exit(1);
	} else {
		printf("RSA_public_decrypt test SUCCEEDED\n");
	}

}

#ifdef TESTReadKeyFile
int main(int argc, char **argv) {
	int result;
	UserStruct User;
	char *HexPriv = NULL, *HexPub = NULL, *HexPython = NULL;
	byte *Byte = NULL;
	byte *PythonData = NULL;
	int PythonLength;
	char *HexByte = NULL;
	
	if (argc != 2) {
		printf("usage: %s keyfile\n", argv[0]);
		exit(1);
	}

	result = ReadKeyFile(argv[1], &User, &HexPriv, &HexPub, &Byte, &HexPython);
	if (result != 0) {
		printf("\nReadKeyFile error %d\n", result);
		exit(result);
	} else {
		printf("\nReadKeyFile OK\n");
	}

	/* just print the rsaPrivString and rsaPubString */
	if (DEBUG) printf("\nrsaPrivString: %s\n", HexPriv);
	if (DEBUG) printf("\nrsaPubString:  %s\n", HexPub);

	/* try to private encrypt-public decrypt something */
	if (DEBUG) pub_priv_test(HexPriv, HexPub);

	/* check all the Byte checksums
	 rehexify it for our Check_All_Byte_Calculus_Data function ... */
	HexByte = Hexify(Byte, 1000);
	if (Check_All_Byte_Calculus_Data(HexByte) != 0) {
		printf("\nByte_Calculus_Data checksums do not match !\n");
		exit(1);
	} else {
		if (DEBUG) printf("\nThe Byte Calculus Data checksums match\n");
	}

	/* Check the KeyPythonPtr */
	PythonLength = strlen(HexPython)/2;
	PythonData = DeHexify(HexPython);
	if (memcmp(PythonData, g_keycode, PythonLength) != 0) {
		printf("\nPython Byte code datablocks do not match !\n");
		exit(1);
	} else {
		if (DEBUG) printf("\nThe Python Byte code datablock matches\n");
	}

	return(0);
}
#else
int main(int argc, char **argv) {
	FILE *rawkeyfile;
	char *AsciiHash;
	char *HexCryptedData, *HexCryptedKey;
	int newlinetracker = 0; /* line position, counts from 0-71 */
	byte *CryptKey;
	char *KeyDataString;
	char *KeyDataPtr;
	char *mdhex;
	char *rsaPrivString;
	char *rsaPubString;
	char *KeyBytePtr;
	char *KeyPythonPtr;
	byte *PythonData;
	int PythonLength;
	UserStruct User;

	if (argc != 2) {
		printf("usage: %s keyfile\n", argv[0]);
		exit(1);
	}

	/* open keyfile for reading */
	if ((rawkeyfile = fopen(argv[1], "r")) == NULL) {
		printf("error, cannot read %s\n", argv[1]);
		exit(1);
	}

	/* Scan and interpret the ASCII part */
	AsciiHash = scan_ascii(rawkeyfile, &User);
	if (DEBUG) printf("\nHexHash: %s\n", AsciiHash);

	/* Read the HexCryptedData */
	HexCryptedData = ReadHexCryptedData(rawkeyfile, &newlinetracker);
	if (DEBUG) printf("\nHexCryptedData: %s\n", HexCryptedData);

	/* Read the HexCryptedKey */
	HexCryptedKey = ReadHexCryptedKey(rawkeyfile, &newlinetracker);
	if (DEBUG) printf("\nHexCryptedKey: %s\n", HexCryptedKey);

	/* close keyfile */
	fclose(rawkeyfile);

	/* Decrypt HexCryptedKey */
	CryptKey = RSADecryptKey(HexCryptedKey);

	/* Decrypt HexCryptedData */
	KeyDataString = DeCryptDatablock(CryptKey, 16, HexCryptedData);
	free(CryptKey);
	free(HexCryptedData);
	free(HexCryptedKey);
	if (DEBUG) printf("\nKeyDataString: %s\n", KeyDataString);

	/* Extract data from KeyDataString */
	KeyDataPtr = KeyDataString;
	
	mdhex         = get_from_datablock(&KeyDataPtr, "01");
	rsaPrivString = get_from_datablock(&KeyDataPtr, "02");
	rsaPubString  = get_from_datablock(&KeyDataPtr, "03");
	KeyBytePtr    = get_from_datablock(&KeyDataPtr, "04");
	KeyPythonPtr  = get_from_datablock(&KeyDataPtr, "05");
	free(KeyDataString);

	/* Check ascii hash */
	if (strcmp(mdhex, AsciiHash) != 0) {
		printf("Ascii part checksums do not match !\n");
		printf("found: %s\n", mdhex);
		printf("check: %s\n", AsciiHash);
		exit(1);
	} else {
		if (DEBUG) printf("\nThe ascii part checksum matches\n");
	}

	/* just print the rsaPrivString and rsaPubString */
	if (DEBUG) printf("\nrsaPrivString: %s\n", rsaPrivString);
	if (DEBUG) printf("\nrsaPubString:  %s\n", rsaPubString);

	/* check all the Byte checksums */
	if (Check_All_Byte_Calculus_Data(KeyBytePtr) != 0) {
		printf("Byte_Calculus_Data checksums do not match !\n");
		exit(1);
	} else {
		if (DEBUG) printf("\nThe Byte Calculus Data checksums match\n");
	}

	/* Check the KeyPythonPtr */
	PythonLength = strlen(KeyPythonPtr)/2;
	PythonData = DeHexify(KeyPythonPtr);
	if (memcmp(PythonData, g_keycode, PythonLength) != 0) {
		printf("Python Byte code datablocks do not match !\n");
		exit(1);
	} else {
		if (DEBUG) printf("\nThe Python Byte code datablock matches\n");
	}

	return(0);
}
#endif
