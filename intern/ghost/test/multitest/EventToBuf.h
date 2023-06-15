/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

char *eventtype_to_string(GHOST_TEventType type);
void event_to_buf(GHOST_EventHandle evt, char buf[128]);
