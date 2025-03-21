/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

struct Main;

/**
 * Called from #do_versions() in `readfile.cc` to convert the old `IPO/adrcode`
 * system to the new Layered Action system.
 *
 * Note: this *only* deals with animation data that is *pre-Animato*, and
 * upgrades it all the way past Animato to modern Layered Actions and drivers.
 * Actions that are already Animato actions are ignored, as they are versioned
 * elsewhere (see `animrig::versioning::convert_legacy_actions()`). This is admittedly
 * weird, but it's due to the fact that versioning pre-Animato data requires
 * creating new datablocks, which must happen at a stage *after* the standard
 * versioning where the simpler Animato-to-Layered upgrades are done.
 *
 * The basic method used here, is to loop over data-blocks which have IPO-data,
 * and add those IPO's to new AnimData blocks as Actions.
 * Action/NLA data only works well for Objects, so these only need to be checked for there.
 *
 * Data that has been converted should be freed immediately, which means that it is immediately
 * clear which data-blocks have yet to be converted, and also prevent freeing errors when we exit.
 *
 * \note Currently done after all file reading.
 */
void do_versions_ipos_to_layered_actions(struct Main *bmain);

/* --------------------- xxx stuff ------------------------ */
