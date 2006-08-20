/*
 * Verse encryption routines. There are two distinct flavors of encryption
 * in use: one "heavy" for login/connection establishment security, and
 * a far lighter symmetrical one that is applied to each data packet after
 * the key has been exchanged during connection.
*/

#include "verse.h"

/* Internal key size definitions. *MUST* be kept in sync with V_HOST_ID_SIZE in verse_header.h! */
#define	V_ENCRYPTION_LOGIN_KEY_BITS	 512
#define V_ENCRYPTION_LOGIN_KEY_SIZE	 (V_ENCRYPTION_LOGIN_KEY_BITS / 8)
#define V_ENCRYPTION_LOGIN_KEY_FULL_SIZE (3 * V_ENCRYPTION_LOGIN_KEY_SIZE)
#define V_ENCRYPTION_LOGIN_KEY_HALF_SIZE (2 * V_ENCRYPTION_LOGIN_KEY_SIZE)

#define V_ENCRYPTION_LOGIN_PUBLIC_START	 (0 * V_ENCRYPTION_LOGIN_KEY_SIZE)
#define V_ENCRYPTION_LOGIN_PRIVATE_START (1 * V_ENCRYPTION_LOGIN_KEY_SIZE)
#define V_ENCRYPTION_LOGIN_N_START	 (2 * V_ENCRYPTION_LOGIN_KEY_SIZE)

#define V_ENCRYPTION_DATA_KEY_SIZE	 (V_ENCRYPTION_LOGIN_KEY_BITS / 8)

/* Connection encryption. Heavy, and symmetrical, so encrypt() does both encryption
 * and decryption given the proper key. Current algorithm used is RSA.
*/
extern void v_e_connect_create_key(uint8 *private_key, uint8 *public_key, uint8 *n);
extern void v_e_connect_encrypt(uint8 *output, const uint8 *data, const uint8 *key, const uint8 *key_n);

/* Actual data traffic encryption. Also symmetrical, with a single key. Uses XOR. */
extern const uint8 *	v_e_data_create_key(void);
extern void		v_e_data_encrypt_command(uint8 *packet, size_t packet_length,
						 const uint8 *command, size_t command_length, const uint8 *key);
extern void		v_e_data_decrypt_packet(uint8 *to, const uint8 *from, size_t size, const uint8 *key);
