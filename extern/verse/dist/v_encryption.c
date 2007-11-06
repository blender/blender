/*
 * Verse encryption routines. Implements RSA encryption/decryption plus fast XORx.
*/

#if !defined(V_GENERATE_FUNC_MODE)

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "verse.h"
#include "v_pack.h"
#include "v_bignum.h"
#include "v_encryption.h"

#define	BITS	V_ENCRYPTION_LOGIN_KEY_BITS	/* Save some typing. */

extern void	v_prime_set_random(VBigDig *x);
extern void	v_prime_set_table(VBigDig *x, int i);

const uint8 * v_e_data_create_key(void) /* possibly the worst key gen ever */
{
	static unsigned int counter = 0;
	static uint8	buffer[V_ENCRYPTION_DATA_KEY_SIZE];
	unsigned int	i, temp;

	for(i = 0; i < V_ENCRYPTION_DATA_KEY_SIZE; i++) 
	{
		counter++;
		temp = (counter << 13) ^ counter;
		temp = (temp * (temp * temp * 15731 + 789221) + 1376312589) & 0x7fffffff;
		buffer[i] = temp;
	}
	/* FIXME: This really isn't very pretty. */
	buffer[0] &= 0x3f;	/* Make sure top word is... Low. For RSA compatibility. */

/*	memset(buffer, 0, sizeof buffer);
	fprintf(stderr, "**WARNING: XOR data encryption disabled\n");
*/
	return buffer;
}

void v_e_data_encrypt_command(uint8 *packet, size_t packet_size, const uint8 *command, size_t command_size, const uint8 *key)
{
	uint32	pos, i;

	vnp_raw_unpack_uint32(packet, &pos);
/*	printf("encrypting packet %u", pos);*/
	pos = key[pos % V_ENCRYPTION_DATA_KEY_SIZE] + packet_size;
/*	printf(" -> pos=%u (size %u)", pos, packet_size);
	printf(", key begins: [");
	for(i = 0; i < 16; i++)
		printf(" %02X", key[(pos + i) % V_ENCRYPTION_DATA_KEY_SIZE]);
	printf(" ]\n");
*/
	for(i = 0; i < command_size; i++)
		packet[packet_size + i] = command[i] ^ key[(i + pos) % V_ENCRYPTION_DATA_KEY_SIZE];
}

void v_e_data_decrypt_packet(uint8 *to, const uint8 *from, size_t size, const uint8 *key)
{
	uint32	pos, i;

	vnp_raw_unpack_uint32(from, &pos);
/*	printf("decrypting packet %u", pos);*/
	pos = key[pos % V_ENCRYPTION_DATA_KEY_SIZE];
/*	printf(" -> pos=%u", pos);
	printf(", key begins: [");
	for(i = 0; i < 16; i++)
		printf(" %02X", key[(i + pos) % V_ENCRYPTION_DATA_KEY_SIZE]);
	printf(" ]\n");
*/	for(i = 0; i < 4; i++)
		to[i] = from[i];
	for(i = 4; i < size; i++)
		to[i] = from[i] ^ key[(i + pos) % V_ENCRYPTION_DATA_KEY_SIZE];
}

#endif

/* From Knuth. Computes multiplicative inverse of u, modulo v. */
void v_e_math_inv(VBigDig *inv, const VBigDig *u, const VBigDig *v)
{
	VBigDig	VBIGNUM(u1, 2*BITS), VBIGNUM(u3, 2*BITS), VBIGNUM(v1, 2*BITS), VBIGNUM(v3, 2 *BITS),
		VBIGNUM(t1, 2*BITS), VBIGNUM(t3, 2*BITS), VBIGNUM(q, 2*BITS),  VBIGNUM(w, 2*BITS);
	int	iter = 1;

	v_bignum_set_one(u1);
	v_bignum_set_bignum(u3, u);
	v_bignum_set_zero(v1);
	v_bignum_set_bignum(v3, v);

	while(!v_bignum_eq_zero(v3))
	{
		v_bignum_set_bignum(q, u3);
		v_bignum_div(q, v3, t3);
		v_bignum_set_bignum(w, q);
		v_bignum_mul(w, v1);
		v_bignum_set_bignum(t1, u1);
		v_bignum_add(t1, w);

		v_bignum_set_bignum(u1, v1);
		v_bignum_set_bignum(v1, t1);
		v_bignum_set_bignum(u3, v3);
		v_bignum_set_bignum(v3, t3);
		iter = -iter;
	}
	if(iter < 0)
	{
		v_bignum_set_bignum(inv, v);
		v_bignum_sub(inv, u1);
	}
	else
		v_bignum_set_bignum(inv, u1);
}

void v_e_connect_create_key(uint8 *private_key, uint8 *public_key, uint8 *n)
{
	VBigDig	VBIGNUM(p, BITS / 2), VBIGNUM(q, BITS / 2), VBIGNUM(qmo, BITS / 2), VBIGNUM(phi, BITS),
		VBIGNUM(pub, BITS), VBIGNUM(priv, BITS), VBIGNUM(mod, BITS);

#if !defined _WIN32
	/* FIXME: This is a security backdoor. Intent is simply to save time during testing. */
	if(getenv("VERSE_NORSA") != NULL)
	{
		printf("VERSE: Found the NORSA envvar, using constant keys\n");
		v_prime_set_table(p, 0);
		v_prime_set_table(q, 1);
		goto compute_phi;
	}
#endif
/*	printf("find prime p\n");*/
	v_prime_set_random(p);
/*	printf("find prime q\n");*/
	v_prime_set_random(q);
compute_phi:
/*	printf("done, computing key\n");*/
/*	printf("p=");
	v_bignum_print_hex_lf(p);
	printf("q=");
	v_bignum_print_hex_lf(q);
*/	v_bignum_set_bignum(phi, p);
	v_bignum_sub_digit(phi, 1);
	v_bignum_set_bignum(qmo, q);
	v_bignum_sub_digit(qmo, 1);
	v_bignum_mul(phi, qmo);
/*	printf("phi=");
	v_bignum_print_hex_lf(phi);
*/	v_bignum_set_string_hex(pub, "0x10001");
	v_e_math_inv(priv, pub, phi);
/*	printf(" pub=");
	v_bignum_print_hex_lf(pub);
	printf("priv=");
	v_bignum_print_hex_lf(priv);
*/
	v_bignum_set_bignum(mod, p);
	v_bignum_mul(mod, q);
/*	printf(" mod=");
	v_bignum_print_hex_lf(mod);
	printf("key-creation finished\n");
*/	/* Write out the keys. */
	v_bignum_raw_export(pub, public_key);
	v_bignum_raw_export(priv, private_key);
	v_bignum_raw_export(mod, n);
}

void v_e_connect_encrypt(uint8 *output, const uint8 *data, const uint8 *key, const uint8 *key_n)
{
	VBigDig	VBIGNUM(packet, BITS), VBIGNUM(expo, BITS), VBIGNUM(mod, BITS);

	v_bignum_raw_import(packet, data);
	v_bignum_raw_import(expo, key);
	v_bignum_raw_import(mod, key_n);

	/* Verify that data is less than the modulo, this is a prerequisite for encryption. */
	if(!v_bignum_gte(mod, packet))
	{
		printf("*** WARNING. Data is not less than modulo, as it should be--encryption will break!\n");
		printf(" RSA modulo: ");
		v_bignum_print_hex_lf(mod);
		printf("   RSA data: ");
		v_bignum_print_hex_lf(packet);
	}
/*	printf("RSA key: ");
	v_bignum_print_hex_lf(expo);
	printf("RSA mod: ");
	v_bignum_print_hex_lf(mod);
	printf("RSA in:  ");
	v_bignum_print_hex_lf(packet);
	printf("bits in packet: %d, ", v_bignum_bit_msb(packet) + 1);
	printf("bits in modulo: %d\n", v_bignum_bit_msb(mod) + 1);
*/	v_bignum_pow_mod(packet, expo, mod);	/* Blam. */
/*	printf("RSA out: ");
	v_bignum_print_hex_lf(packet);
*/	v_bignum_raw_export(packet, output);
}

#if defined CRYPTALONE
void v_encrypt_test(void)
{
	uint8	k_priv[BITS / 8], k_pub[BITS / 8], k_n[BITS / 8], cipher[BITS / 8], plain[BITS / 8], decode[BITS / 8], i;

	printf("testing RSA-crypto\n");
	v_e_connect_create_key(k_pub, k_priv, k_n);
/*	exit(0);*/
	printf("key pair generated, encrypting something\n");
	memset(plain, 0, sizeof plain);
	strcpy(plain, "This is some text to encrypt, to give it something to chew on.");
	printf("plain: %02X (%u)\n", plain[0], strlen(plain));
	v_e_connect_encrypt(cipher, plain, k_pub, k_n);
	printf("plain: %02X, cipher: %02X\n", plain[0], cipher[0]);
	v_e_connect_encrypt(decode, cipher, k_priv, k_n);
	printf("decoded: %02X: '", decode[0]);
	for(i = 0; decode[i] != 0; i++)
		putchar(decode[i]);
	printf("'\n");
/*	printf("\npublic key: ");
	v_bignum_print_hex_lf(k_public);
	printf("private key: ");
	v_bignum_print_hex_lf(k_private);
	v_bignum_set_string(msg, "123");
	gettimeofday(&t1, NULL);
	v_bignum_pow_mod(msg, k_private, k_n);
	gettimeofday(&t2, NULL);
	printf("encrypted: ");
	v_bignum_print_hex_lf(msg);
	printf("encrypted %u bits in %g s\n", BITS, t2.tv_sec - t1.tv_sec + 1.0E-6 * (t2.tv_usec - t1.tv_usec));

	gettimeofday(&t1, NULL);
	v_bignum_pow_mod(msg, k_public, k_n);
	gettimeofday(&t2, NULL);
	printf("decrypted: ");
	v_bignum_print_hex_lf(msg);
	printf("decrypted %u bits in %g s\n", BITS, t2.tv_sec - t1.tv_sec + 1.0E-6 * (t2.tv_usec - t1.tv_usec));
	exit(0);
*//*	v_e_encrypt(cipher, plain, &k_private, &k_n);
	printf("encrypted data: ");
	for(i = 0; i < sizeof cipher; i++)
		printf("%c", isalnum(cipher[i]) ? cipher[i] : '?');
	printf("\n\n");
	printf("decrypting\n");
	v_e_encrypt(decode, cipher, &k_public, &k_n);
	printf("decrypted data: ");
	for(i = 0; i < sizeof cipher; i++)
		printf("%c", isalnum(decode[i]) ? decode[i] : '?');
	printf("\n\n");
*/
}

int main(void)
{
	v_encrypt_test();

	return 0;
}
#endif
