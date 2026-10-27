/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_GestureHandlerCocoa.hh"

#include "GHOST_EventButton.hh"
#include "GHOST_EventCursor.hh"
#include "GHOST_EventTrackpad.hh"
#include "GHOST_SystemCocoa.hh"
#include "GHOST_WindowCocoa.hh"
#include "GHOST_utildefines.hh"

/* --------------------------------------------------------------------
 * This BlenderContactObserver NSGestureRecognizer subclass provides position tracking for direct
 * touch contacts, allowing them to be tracked separately of any other gesture-specific recognizer.
 *
 * These contacts are then used for positioning the mouse cursor, providing the average contact
 * midpoint on multi-finger gestures. For more details, see its `contactObserverAction` action
 * in GHOST_GestureHandlerCocoa.
 */
@interface BlenderContactObserver : NSGestureRecognizer
@property(nonatomic, readonly) NSUInteger touchCount;
@end

@implementation BlenderContactObserver
{
  NSPoint average_contact_location_;
  NSUInteger touch_count_;
}

@synthesize touchCount = touch_count_;

- (void)updateTouches:(NSEvent *)event
{
  NSPoint location_sum = NSZeroPoint;
  touch_count_ = 0;

  NSSet<NSTouch *> *touches = [event touchesMatchingPhase:NSTouchPhaseTouching inView:self.view];

  for (NSTouch *touch in touches) {
    touch_count_++;

    const NSPoint point = [touch locationInView:self.view];
    location_sum.x += point.x;
    location_sum.y += point.y;
  }

  if (touch_count_ != 0) {
    average_contact_location_ = NSMakePoint(location_sum.x / touch_count_,
                                            location_sum.y / touch_count_);
  }
}

- (void)touchesBeganWithEvent:(NSEvent *)event
{
  [super touchesBeganWithEvent:event];
  [self updateTouches:event];

  self.state = (self.state == NSGestureRecognizerStatePossible) ? NSGestureRecognizerStateBegan :
                                                                  NSGestureRecognizerStateChanged;
}

- (void)touchesMovedWithEvent:(NSEvent *)event
{
  [super touchesMovedWithEvent:event];
  [self updateTouches:event];

  self.state = NSGestureRecognizerStateChanged;
}

- (void)touchesEndedWithEvent:(NSEvent *)event
{
  [super touchesEndedWithEvent:event];
  [self updateTouches:event];

  self.state = (touch_count_ == 0) ? NSGestureRecognizerStateEnded :
                                     NSGestureRecognizerStateChanged;
}

- (void)touchesCancelledWithEvent:(NSEvent *)event
{
  [super touchesCancelledWithEvent:event];
  [self updateTouches:event];

  self.state = NSGestureRecognizerStateCancelled;
}

- (NSPoint)locationInView:(NSView *)view
{
  return [self.view convertPoint:average_contact_location_ toView:view];
}

- (void)reset
{
  [super reset];

  /* Do not clear contact_location_ here as other recognizer actions that could still
   * execute may still need the last contact position. */
  touch_count_ = 0;
}

@end

/*--------------------------------------------------------------------
 * This custom pan gesture recognizer subclass keeps recognition pending until enough fingers
 * arrive, preventing the recognizer from failing early if movement starts before the required
 * number of touches is reached. In addition to this, adding extra touches cancels the active
 * gesture, allowing a one-finger drag to turn into a two-finger pan.
 *
 * Use the now deprecated numberOfTouchesRequired property instead of the newer macOS 27
 * minimum/maximumNumberOfTouches ones for Xcode <= 26 support. */
@interface BlenderPanRecognizer : NSPanGestureRecognizer
@end

@implementation BlenderPanRecognizer

- (NSInteger)getActiveTouchCount:(NSEvent *)event
{
  return [event touchesMatchingPhase:NSTouchPhaseTouching inView:self.view].count;
}

- (void)touchesBeganWithEvent:(NSEvent *)event
{
  if ([self getActiveTouchCount:event] > self.numberOfTouchesRequired) {
    /* On finger contact, cancel the gesture if we exceed the required touch count. */
    switch (self.state) {
      case NSGestureRecognizerStatePossible:
        self.state = NSGestureRecognizerStateFailed;
        break;
      case NSGestureRecognizerStateBegan:
      case NSGestureRecognizerStateChanged:
        self.state = NSGestureRecognizerStateCancelled;
        break;
      default:
        break;
    }
    return;
  }
  [super touchesBeganWithEvent:event];
}

- (void)touchesMovedWithEvent:(NSEvent *)event
{
  if (self.state == NSGestureRecognizerStatePossible) {
    if ([self getActiveTouchCount:event] < self.numberOfTouchesRequired) {
      /* Keep tracking contacts without recognizing the gesture before enough fingers arrive. */
      return;
    }
  }
  [super touchesMovedWithEvent:event];
}

@end

/*--------------------------------------------------------------------
 * This Gesture Handler class owns and coordinates Gesture Recognizers for a given window, acting
 * as the recognizers' target and delegate, translating Sidecar touch gestures into GHOST events.
 *
 * The different gesture recognizers are allowed to run simulatenously. For continuous gestures,
 * the recognizers actions are only in charge of pressing the left-click down. The mouse cursor
 * location and final left click release being handled by the contact observer recognizer.
 */
@implementation GHOST_GestureHandlerCocoa
{
  /* GHOST system, window, and macOS NSView passed on initialization. */
  GHOST_SystemCocoa *system_cocoa_;
  GHOST_WindowCocoa *window_cocoa_;
  NSView *view_;

  /* List of managed recognizers. */
  NSMutableArray<NSGestureRecognizer *> *recognizers_;

  /* Contact observer recognizer providing conitnuous position of all touch contacts. */
  BlenderContactObserver *contact_observer_;

  /* Left button state shared by press and drag gestures. */
  bool left_click_pressed_;

  /* Recognizer values from previous action, used to calculate incremental delta changes. */
  double magnify_prev_;
  double rotate_prev_;

  /* Fractional GHOST event delta remainders carried onto the next update. */
  double pan_rem_[2];
  double magnify_rem_;
  double rotate_rem_;
}

- (void)addRecognizer:(NSGestureRecognizer *)recognizer
{
  /* Only direct Sidecar touch events are handled, see the shouldAttemptToRecognizeWithEvent
   * method for more details. */
  recognizer.allowedTouchTypes = NSTouchTypeMaskDirect;

  recognizer.delegate = self;

  [recognizers_ addObject:recognizer];
  [view_ addGestureRecognizer:recognizer];

  /* Balances the alloc, recognizer is now owned by the recognizer_ array and the view. */
  [recognizer release];
}

- (instancetype)initWithSystemCocoa:(GHOST_SystemCocoa *)system
                        windowCocoa:(GHOST_WindowCocoa *)window
                               view:(NSView *)view
{
  self = [super init];

  if (self) {
    system_cocoa_ = system;
    window_cocoa_ = window;
    view_ = [view retain];

    recognizers_ = [[NSMutableArray alloc] init];

    /* Touch contact observer. */
    contact_observer_ = [[BlenderContactObserver alloc]
        initWithTarget:self
                action:@selector(contactObserverAction:)];
    [self addRecognizer:contact_observer_];

    /* Tap / click recognizer. */
    NSClickGestureRecognizer *click_recognizer = [[NSClickGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(tapAction:)];
    click_recognizer.numberOfClicksRequired = 1;
    [self addRecognizer:click_recognizer];

    /* Press / hold recognizer. */
    NSPressGestureRecognizer *press_recognizer = [[NSPressGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(pressAction:)];
    /* Recognize a fixed press / hold after a short delay, allowing small finger movements.
     * Values based on the corresponding UIKit UILongPressGestureRecognizer property defaults. */
    press_recognizer.minimumPressDuration = 0.5;
    press_recognizer.allowableMovement = 10.0;
    [self addRecognizer:press_recognizer];

    /* Panning / dragging recognizers. */
    /* One-finger click dragging. */
    BlenderPanRecognizer *drag_recognizer = [[BlenderPanRecognizer alloc]
        initWithTarget:self
                action:@selector(dragAction:)];
    drag_recognizer.numberOfTouchesRequired = 1;
    [self addRecognizer:drag_recognizer];

    /* Two-finger viewport pan navigation. */
    BlenderPanRecognizer *pan_recognizer = [[BlenderPanRecognizer alloc]
        initWithTarget:self
                action:@selector(panAction:)];
    pan_recognizer.numberOfTouchesRequired = 2;
    [self addRecognizer:pan_recognizer];

    /* Magnification / Rotation recognizers. */
    [self addRecognizer:[[NSMagnificationGestureRecognizer alloc]
                            initWithTarget:self
                                    action:@selector(magnifyAction:)]];
    [self addRecognizer:[[NSRotationGestureRecognizer alloc]
                            initWithTarget:self
                                    action:@selector(rotateAction:)]];
  }

  return self;
}

- (void)dealloc
{
  for (NSGestureRecognizer *recognizer in recognizers_) {
    recognizer.target = nil;
    recognizer.delegate = nil;
    [view_ removeGestureRecognizer:recognizer];
  }

  [recognizers_ release];
  [view_ release];
  [super dealloc];
}

/**
 * NSGestureRecognizerDelegate methods implementations, used by all attached recognizers.
 */

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)recognizer
    shouldAttemptToRecognizeWithEvent:(NSEvent *)event
{
  /* Reject mouse and trackpad events for all attached recognizers, leaving them to be separately
   * handled by the NSResponder sendEvent stream (see SystemCocoa.mm and WindowViewCocoa.hh). */
  return NO;
}

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)recognizer
    shouldRequireFailureOfGestureRecognizer:(NSGestureRecognizer *)other
{
  /* Give the press and one-finger drag recognizers priority over the tap / click recognizer. */
  return recognizer.action == @selector(tapAction:) && other.target == self &&
         ELEM(other.action, @selector(pressAction:), @selector(dragAction:));
}

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)recognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(NSGestureRecognizer *)other
{
  /* Allow attached recognizers to recognize gestures simultaneously. */
  return YES;
}

/**
 * Utility methods for submitting GHOST events and handling mouse input from touch contacts.
 */

- (int32_t)computeGestureDelta:(double)delta remainder:(double &)remainder
{
  const double value = delta + remainder;
  const int32_t whole = int32_t(value);

  remainder = value - whole;

  return whole;
}

- (void)pushEvent:(std::unique_ptr<const GHOST_IEvent>)event
{
  system_cocoa_->pushEvent(std::move(event));
  system_cocoa_->notifyExternalEventProcessed();
}

- (NSPoint)screenPositionFromContact
{
  const NSPoint point_in_view = [contact_observer_ locationInView:view_];
  const NSPoint point_in_window = [view_ convertPoint:point_in_view toView:nil];

  NSWindow *window = static_cast<NSWindow *>(window_cocoa_->getOSWindow());
  return [window convertPointToScreen:point_in_window];
}

- (void)moveCursorToContact
{
  const NSPoint position = [self screenPositionFromContact];
  [self pushEvent:std::make_unique<GHOST_EventCursor>(system_cocoa_->getMilliSeconds(),
                                                      GHOST_kEventCursorMove,
                                                      window_cocoa_,
                                                      int32_t(position.x),
                                                      int32_t(position.y),
                                                      GHOST_TABLET_DATA_NONE)];
}

- (void)pressLeftClick
{
  if (!left_click_pressed_) {
    [self pushEvent:std::make_unique<GHOST_EventButton>(system_cocoa_->getMilliSeconds(),
                                                        GHOST_kEventButtonDown,
                                                        window_cocoa_,
                                                        GHOST_kButtonMaskLeft,
                                                        GHOST_TABLET_DATA_NONE)];
  }
  left_click_pressed_ = true;
}

- (void)releaseLeftClick
{
  if (left_click_pressed_) {
    [self pushEvent:std::make_unique<GHOST_EventButton>(system_cocoa_->getMilliSeconds(),
                                                        GHOST_kEventButtonUp,
                                                        window_cocoa_,
                                                        GHOST_kButtonMaskLeft,
                                                        GHOST_TABLET_DATA_NONE)];
    left_click_pressed_ = false;
  }
}

/**
 * Gesture Recognizer actions.
 */

- (void)contactObserverAction:(BlenderContactObserver *)recognizer
{
  /* Workaround: If the Blender window isn't focused, Sidecar touch input will not cause the window
   * to become key/focused nor Blender to activate. This causes issues with widget and tool
   * dragging (among others). Workaround this issue by ordering the window front on touch input. */
  window_cocoa_->setOrder(GHOST_kWindowOrderTop);

  switch (recognizer.state) {
    case NSGestureRecognizerStateBegan:
    case NSGestureRecognizerStateChanged:
      if (recognizer.touchCount == 0) {
        return;
      }

      if (recognizer.touchCount > 1) {
        /* When switching from one-finger dragging to two-finger panning, ensure the left-click
         * is released before moving the cursor to the mid-point between the two contacts.
         *
         * This is required as the contactObserverAction may run before the dragAction cancel
         * while transitioning between the two gestures. */
        [self releaseLeftClick];
      }

      /* Position the mouse cursor, using the average contact center for multi-finger touches. */
      [self moveCursorToContact];

      return;

    default:
      return;
  }
}

- (void)tapAction:(NSClickGestureRecognizer *)recognizer
{
  [self pressLeftClick];
  [self releaseLeftClick];
}

- (void)pressAction:(NSPressGestureRecognizer *)recognizer
{
  switch (recognizer.state) {
    case NSGestureRecognizerStateBegan:
      [self pressLeftClick];
      return;

    case NSGestureRecognizerStateEnded:
    case NSGestureRecognizerStateCancelled:
      [self releaseLeftClick];
      return;

    default:
      return;
  }
}

- (void)dragAction:(NSPanGestureRecognizer *)recognizer
{
  switch (recognizer.state) {
    case NSGestureRecognizerStateBegan:
      [self pressLeftClick];
      return;

    case NSGestureRecognizerStateEnded:
    case NSGestureRecognizerStateCancelled:
      [self releaseLeftClick];
      return;

    default:
      return;
  }
}

- (void)panAction:(NSPanGestureRecognizer *)recognizer
{
  switch (recognizer.state) {
    case NSGestureRecognizerStateBegan:
      pan_rem_[0] = 0.0;
      pan_rem_[1] = 0.0;
      return;

    case NSGestureRecognizerStateChanged:
      /* Execute the action. */
      break;

    default:
      return;
  }

  const NSPoint translation = [recognizer translationInView:view_];

  /* Reset cumulative translation to only get the delta difference on the next action. */
  [recognizer setTranslation:NSZeroPoint inView:view_];

  /* Convert movement to backing pixels, reverse Y to match GHOST scroll events. */
  const NSPoint delta = [view_ convertPointToBacking:NSMakePoint(translation.x, -translation.y)];

  const int32_t dx = [self computeGestureDelta:delta.x remainder:pan_rem_[0]];
  const int32_t dy = [self computeGestureDelta:delta.y remainder:pan_rem_[1]];

  const NSPoint position = [self screenPositionFromContact];

  /* Forward two-finger panning via GHOST Trackpad scrolling events. */
  [self pushEvent:std::make_unique<GHOST_EventTrackpad>(system_cocoa_->getMilliSeconds(),
                                                        window_cocoa_,
                                                        GHOST_kTrackpadEventScroll,
                                                        int32_t(position.x),
                                                        int32_t(position.y),
                                                        dx,
                                                        dy,
                                                        true)];
}

- (void)magnifyAction:(NSMagnificationGestureRecognizer *)recognizer
{
  switch (recognizer.state) {
    case NSGestureRecognizerStateBegan:
      magnify_prev_ = recognizer.magnification;
      magnify_rem_ = 0.0;
      return;

    case NSGestureRecognizerStateChanged:
    case NSGestureRecognizerStateEnded:
      /* Execute the action. */
      break;

    default:
      return;
  }

  const double delta = recognizer.magnification - magnify_prev_;
  magnify_prev_ = recognizer.magnification;

  /* Using the same manignification constant as existing trackpad code (in SystemCocoa.mm). */
  const int32_t whole = [self computeGestureDelta:delta * 125.0 remainder:magnify_rem_];

  if (whole != 0) {
    const NSPoint position = [self screenPositionFromContact];
    [self pushEvent:std::make_unique<GHOST_EventTrackpad>(system_cocoa_->getMilliSeconds(),
                                                          window_cocoa_,
                                                          GHOST_kTrackpadEventMagnify,
                                                          int32_t(position.x),
                                                          int32_t(position.y),
                                                          whole,
                                                          0,
                                                          false)];
  }

  if (recognizer.state == NSGestureRecognizerStateEnded) {
    magnify_rem_ = 0.0;
  }
}

- (void)rotateAction:(NSRotationGestureRecognizer *)recognizer
{
  switch (recognizer.state) {
    case NSGestureRecognizerStateBegan:
      rotate_prev_ = recognizer.rotation;
      rotate_rem_ = 0.0;
      return;

    case NSGestureRecognizerStateChanged:
    case NSGestureRecognizerStateEnded:
      /* Execute the action. */
      break;

    default:
      return;
  }

  const double delta = recognizer.rotation - rotate_prev_;
  rotate_prev_ = recognizer.rotation;

  /* Using the same rotation speed constant as existing trackpad code (in SystemCocoa.mm). */
  const int32_t whole = [self computeGestureDelta:delta * (180.0 / M_PI) * -5.0
                                        remainder:rotate_rem_];

  if (whole != 0) {
    const NSPoint position = [self screenPositionFromContact];
    [self pushEvent:std::make_unique<GHOST_EventTrackpad>(system_cocoa_->getMilliSeconds(),
                                                          window_cocoa_,
                                                          GHOST_kTrackpadEventRotate,
                                                          int32_t(position.x),
                                                          int32_t(position.y),
                                                          whole,
                                                          0,
                                                          false)];
  }

  if (recognizer.state == NSGestureRecognizerStateEnded) {
    rotate_rem_ = 0.0;
  }
}

@end
