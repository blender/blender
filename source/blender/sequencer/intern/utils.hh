/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include <memory>

namespace blender {

struct MovieReader;
struct Strip;

namespace seq {

bool sequencer_strip_generates_image(Strip *strip);

struct MovieReaderDeleter {
  void operator()(MovieReader *reader) const;
};
using MovieReaderPtr = std::unique_ptr<MovieReader, MovieReaderDeleter>;

void movie_metadata_set_from_reader(Strip &strip, MovieReader &reader);

}  // namespace seq
}  // namespace blender
