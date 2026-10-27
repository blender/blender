/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#import <Cocoa/Cocoa.h>

class GHOST_SystemCocoa;
class GHOST_WindowCocoa;

@interface GHOST_GestureHandlerCocoa : NSObject <NSGestureRecognizerDelegate>

- (instancetype)initWithSystemCocoa:(GHOST_SystemCocoa *)system
                        windowCocoa:(GHOST_WindowCocoa *)window
                               view:(NSView *)view;

@end
