/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_CallbackEventConsumer.h"
#include "GHOST_C-api.h"
#include "GHOST_Debug.h"

GHOST_CallbackEventConsumer::GHOST_CallbackEventConsumer(GHOST_EventCallbackProcPtr eventCallback,
                                                         GHOST_TUserDataPtr userData)
{
  m_eventCallback = eventCallback;
  m_userData = userData;
}

bool GHOST_CallbackEventConsumer::processEvent(GHOST_IEvent *event)
{
  return m_eventCallback((GHOST_EventHandle)event, m_userData);
}
