/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief Efficient in-memory storage of multiple similar arrays.
 */

#include "BLI_sys_types.h"

typedef struct BArrayState BArrayState;
typedef struct BArrayStore BArrayStore;

/**
 * Create a new array store, which can store any number of arrays
 * as long as their stride matches.
 *
 * \param stride: `sizeof()` each element,
 *
 * \note while a stride of `1` will always work,
 * its less efficient since duplicate chunks of memory will be searched
 * at positions unaligned with the array data.
 *
 * \param chunk_count: Number of elements to split each chunk into.
 * - A small value increases the ability to de-duplicate chunks,
 *   but adds overhead by increasing the number of chunks to look up when searching for duplicates,
 *   as well as some overhead constructing the original array again, with more calls to `memcpy`.
 * - Larger values reduce the *book keeping* overhead,
 *   but increase the chance a small,
 *   isolated change will cause a larger amount of data to be duplicated.
 *
 * \return A new array store, to be freed with #BLI_array_store_destroy.
 */
BArrayStore *BLI_array_store_create(unsigned int stride, unsigned int chunk_count);
/**
 * Free the #BArrayStore, including all states and chunks.
 */
void BLI_array_store_destroy(BArrayStore *bs);
/**
 * Clear all contents, allowing reuse of \a bs.
 */
void BLI_array_store_clear(BArrayStore *bs);

/**
 * Find the memory used by all states (expanded & real).
 *
 * \return the total amount of memory that would be used by getting the arrays for all states.
 */
size_t BLI_array_store_calc_size_expanded_get(const BArrayStore *bs);
/**
 * \return the amount of memory used by all #BChunk.data
 * (duplicate chunks are only counted once).
 */
size_t BLI_array_store_calc_size_compacted_get(const BArrayStore *bs);

/**
 * \param data: Data used to create
 * \param state_reference: The state to use as a reference when adding the new state,
 * typically this is the previous state,
 * however it can be any previously created state from this \a bs.
 *
 * \return The new state,
 * which is used by the caller as a handle to get back the contents of \a data.
 * This may be removed using #BLI_array_store_state_remove,
 * otherwise it will be removed with #BLI_array_store_destroy.
 */
BArrayState *BLI_array_store_state_add(BArrayStore *bs,
                                       const void *data,
                                       size_t data_len,
                                       const BArrayState *state_reference);
/**
 * Remove a state and free any unused #BChunk data.
 *
 * The states can be freed in any order.
 */
void BLI_array_store_state_remove(BArrayStore *bs, BArrayState *state);

/**
 * \return the expanded size of the array,
 * use this to know how much memory to allocate #BLI_array_store_state_data_get's argument.
 */
size_t BLI_array_store_state_size_get(const BArrayState *state);
/**
 * Fill in existing allocated memory with the contents of \a state.
 */
void BLI_array_store_state_data_get(const BArrayState *state, void *data);
/**
 * Allocate an array for \a state and return it.
 */
void *BLI_array_store_state_data_get_alloc(const BArrayState *state, size_t *r_data_len);

/**
 * \note Only for tests.
 */
bool BLI_array_store_is_valid(BArrayStore *bs);

/* `array_store_rle.cc` */

/**
 * Return a run-length encoded copy of `data_dec`.
 *
 * \param data_dec: The data to encode.
 * \param data_dec_len: The size of the data to encode.
 * \param data_enc_extra_size: Allocate extra memory at the beginning of the array.
 * - This doesn't impact the value of `r_data_enc_len`.
 * - This must be skipped when decoding.
 * \param r_data_enc_len: The size of the resulting RLE encoded data.
 */
uint8_t *BLI_array_store_rle_encode(const uint8_t *data_dec,
                                    size_t data_dec_len,
                                    size_t data_enc_extra_size,
                                    size_t *r_data_enc_len);
/**
 *  Decode a run-length encoded array, writing the result into `data_dec_v`.
 *
 * \param data_enc: The data to encode (returned by #BLI_array_store_rle_encode).
 * \param data_enc_len: The size of `data_enc`.
 * \param data_dec: The destination for the decoded data to be written to.
 * \param data_dec_len: The size of the destination (as passed to #BLI_array_store_rle_encode).
 */
void BLI_array_store_rle_decode(const uint8_t *data_enc,
                                const size_t data_enc_len,
                                void *data_dec_v,
                                const size_t data_dec_len);
