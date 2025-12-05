/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
FileReader *BLO_file_reader_uncompressed_from_descriptor(int filedes);
FileReader *BLO_file_reader_uncompressed_from_memory(const void *mem, const int memsize);
