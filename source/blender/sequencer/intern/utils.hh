/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ListBase;
struct Scene;

bool sequencer_seq_generates_image(Strip *seq);
void seq_open_anim_file(Scene *scene, Strip *seq, bool openfile);
