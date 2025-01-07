/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct Scene;
struct Strip;

Strip *SEQ_select_active_get(const Scene *scene);
bool SEQ_select_active_get_pair(Scene *scene, Strip **r_seq_act, Strip **r_seq_other);
void SEQ_select_active_set(Scene *scene, Strip *strip);
