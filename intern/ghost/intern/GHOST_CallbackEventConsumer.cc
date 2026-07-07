/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_CallbackEventConsumer.hh"

GHOST_CallbackEventConsumer::GHOST_CallbackEventConsumer(GHOST_EventCallbackProcPtr eventCallback,
                                                         GHOST_TUserDataPtr user_data)
{
  event_callback_ = eventCallback;
  user_data_ = user_data;
}

bool GHOST_CallbackEventConsumer::processEvent(const GHOST_IEvent *event)
{
  return event_callback_(event, user_data_);
}
