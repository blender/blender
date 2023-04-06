/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Types.h"

class GHOST_XrSession;

class GHOST_IXrContext {
 public:
  virtual ~GHOST_IXrContext() = default;

  virtual void startSession(const GHOST_XrSessionBeginInfo *begin_info) = 0;
  virtual void endSession() = 0;
  virtual bool isSessionRunning() const = 0;
  virtual void drawSessionViews(void *draw_customdata) = 0;

  /* Needed for the GHOST C api. */
  virtual GHOST_XrSession *getSession() = 0;
  virtual const GHOST_XrSession *getSession() const = 0;

  virtual void dispatchErrorMessage(const class GHOST_XrException *) const = 0;

  virtual void setGraphicsContextBindFuncs(GHOST_XrGraphicsContextBindFn bind_fn,
                                           GHOST_XrGraphicsContextUnbindFn unbind_fn) = 0;
  virtual void setDrawViewFunc(GHOST_XrDrawViewFn draw_view_fn) = 0;

  virtual bool needsUpsideDownDrawing() const = 0;
};
