/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */
#pragma once

/* The CacheArchiveHandle struct is only used for anonymous pointers,
 * to interface between C and C++ code. This is currently used
 * to hide pointers to alembic ArchiveReader and USDStageReader. */
struct CacheArchiveHandle {
  int unused;
};

/* The CacheReader struct is only used for anonymous pointers,
 * to interface between C and C++ code. This is currently used
 * to hide pointers to AbcObjectReader and USDPrimReader
 * (or subclasses thereof). */
struct CacheReader {
  int unused;
};
