/*
** v_pack.h
**
** These functions are used to pack and unpack various quantities to/from network
** packet buffers. They do not care about alignment, operating at byte level internally.
** The external byte-ordering used is big-endian (aka "network byte order") for all
** quantities larger than a single byte.
*/

#include "verse_header.h"

extern size_t vnp_raw_pack_uint8(void *buffer, uint8 data);
extern size_t vnp_raw_unpack_uint8(const void *buffer, uint8 *data);

extern size_t vnp_raw_pack_uint16(void *buffer, uint16 data);
extern size_t vnp_raw_unpack_uint16(const void *buffer, uint16 *data);

extern size_t vnp_raw_pack_uint24(void *buffer, uint32 data);
extern size_t vnp_raw_unpack_uint24(const void *buffer, uint32 *data);

extern size_t vnp_raw_pack_uint32(void *buffer, uint32 data);
extern size_t vnp_raw_unpack_uint32(const void *buffer, uint32 *data);

extern size_t vnp_raw_pack_real32(void *buffer, real32 data);
extern size_t vnp_raw_unpack_real32(const void *buffer, real32 *data);

extern size_t vnp_raw_pack_real64(void *buffer, real64 data);
extern size_t vnp_raw_unpack_real64(const void *buffer, real64 *data);

extern size_t vnp_raw_pack_string(void *buffer, const char *string, size_t max_size);
extern size_t vnp_raw_unpack_string(const void *buffer, char *string, size_t max_size, size_t max_size2);

extern size_t vnp_raw_pack_uint8_vector(void *buffer, const uint8 *data, unsigned int length);
extern size_t vnp_raw_unpack_uint8_vector(const void *buffer, uint8 *data, unsigned int length);

extern size_t vnp_raw_pack_uint16_vector(void *buffer, const uint16 *data, unsigned int length);
extern size_t vnp_raw_unpack_uint16_vector(const void *buffer, uint16 *data, unsigned int length);

extern size_t vnp_raw_pack_uint24_vector(void *buffer, const uint32 *data, unsigned int length);
extern size_t vnp_raw_unpack_uint24_vector(const void *buffer, uint32 *data, unsigned int length);

extern size_t vnp_raw_pack_uint32_vector(void *buffer, const uint32 *data, unsigned int length);
extern size_t vnp_raw_unpack_uint32_vector(const void *buffer, uint32 *data, unsigned int length);

extern size_t vnp_raw_pack_real32_vector(void *buffer, const real32 *data, unsigned int length);
extern size_t vnp_raw_unpack_real32_vector(const void *buffer, real32 *data, unsigned int length);

extern size_t vnp_raw_pack_real64_vector(void *buffer, const real64 *data, unsigned int length);
extern size_t vnp_raw_unpack_real64_vector(const void *buffer, real64 *data, unsigned int length);

/* --------------------------------------------------------------------------------------------------- */

extern size_t vnp_pack_quat32(void *buffer, const VNQuat32 *data);
extern size_t vnp_unpack_quat32(const void *buffer, VNQuat32 *data);
extern size_t vnp_pack_quat64(void *buffer, const VNQuat64 *data);
extern size_t vnp_unpack_quat64(const void *buffer, VNQuat64 *data);

extern size_t vnp_pack_audio_block(void *buffer, VNABlockType type, const VNABlock *block);
extern size_t vnp_unpack_audio_block(const void *buffer, VNABlockType type, VNABlock *block);
