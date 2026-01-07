/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {

struct FileReader;

/**
 * .blend files can be optionally compressed. This function takes a file reader and either returns
 * it directly or returns a decompressing reader.
 *
 * Ownership of the rawfile is transferred to the function. The returned reader may be the same or
 * may wrap the provided reader.
 *
 * \return null if the file is detected to not be a blend file.
 */
FileReader *BLO_file_reader_uncompressed(FileReader *rawfile);

/**
 * Same as #BLO_file_reader_uncompressed but uses a file descriptor. Ownership of the descriptor
 * is passed to the function. So the file will be closed if it's not a valid blend file.
 */
FileReader *BLO_file_reader_uncompressed_from_descriptor(int filedes);

/**
 * Same as #BLO_file_reader_uncompressed but directly reads from an existing buffer. Ownership of
 * the memory is *not* passed to the function. So the caller remains responsible for freeing it.
 */
FileReader *BLO_file_reader_uncompressed_from_memory(const void *mem, const int memsize);

}  // namespace blender
