/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GHOST_WindowIOS.h"

#include "GHOST_ContextIOS.hh"
#include "GHOST_SystemIOS.h"

#include "GHOST_C-api.h"
#include "GHOST_Debug.hh"
#include "GHOST_EventButton.hh"
#include "GHOST_EventCursor.hh"
#include "GHOST_EventDragnDrop.hh"
#include "GHOST_EventKey.hh"
#include "GHOST_EventTouch.hh"
#include "GHOST_EventTrackpad.hh"

#import <GameController/GameController.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <UIKit/UIPencilInteraction.h>

#include <unordered_map>

// #define IOS_INPUT_LOGGING
#if defined(IOS_INPUT_LOGGING)
#  define IOS_INPUT_LOG(...) NSLog(__VA_ARGS__)
#else
#  define IOS_INPUT_LOG(...)
#endif

// #define IOS_WINDOW_LOGGING
#if defined(IOS_WINDOW_LOGGING)
#  define IOS_WINDOW_LOG(...) NSLog(__VA_ARGS__)
#else
#  define IOS_WINDOW_LOG(...)
#endif

struct TouchData {
  CGPoint pos;
  bool part_of_multitouch = false;
};

typedef struct UserInputEvent {
  enum EventTypes {
    CURSOR_MOVE,
    PAN_GESTURE,
    PAN_GESTURE_TWO_FINGERS,
    PINCH_GESTURE,
    LEFT_BUTTON_DOWN,
    LEFT_BUTTON_UP,
    PENCIL_TAP,
  };
  EventTypes event_list[10];
  int num_events;
  CGPoint location;
  CGPoint translation;
  CGFloat distance;
  bool pencil_used;

  UserInputEvent(CGPoint *loc, CGPoint *tran, CGFloat *dist, bool pencil)
  {
    num_events = 0;
    location = loc ? *loc : CGPointMake(-1.0f, -1.0f);
    translation = tran ? *tran : CGPointMake(0.0f, 0.0f);
    distance = dist ? *dist : 0.0f;
    pencil_used = pencil;
  }

  void add_event(EventTypes event_type)
  {
    GHOST_ASSERT(num_events <= sizeof(event_list) / sizeof(*event_list),
                 "add_event: Failed to add event");
    event_list[num_events] = event_type;
    num_events++;
  }

  NSString *getEventTypeDesc(EventTypes event_type) const
  {
    switch (event_type) {
      case CURSOR_MOVE:
        return @"CM";
      case PAN_GESTURE:
        return @"PAN";
      case PAN_GESTURE_TWO_FINGERS:
        return @"PAN2F";
      case PINCH_GESTURE:
        return @"PINCH";
      case LEFT_BUTTON_DOWN:
        return @"LB-DOWN";
      case LEFT_BUTTON_UP:
        return @"LB-UP";
      case PENCIL_TAP:
        return @"PENCIL-TAP";
    }
    BLI_assert_unreachable();
    return @"Event undefined";
  }

} UserInputEvent;

/* GHOSTUITapGesture interface for capturing taps. */
@interface GHOSTUITapGestureRecognizer : UITapGestureRecognizer

- (CGPoint)getScaledTouchPoint:(GHOST_WindowIOS *)window;

@end

@implementation GHOSTUITapGestureRecognizer

- (CGPoint)getScaledTouchPoint:(GHOST_WindowIOS *)window
{
  CGPoint touch_point = [self locationInView:window->getView()];
  return window->scalePointToWindow(touch_point);
}

@end

/* GHOSTUITapGesture interface for capturing taps. */
@interface GHOSTUIPanGestureRecognizer : UIPanGestureRecognizer
{
  CGPoint cached_translation;
}
- (CGPoint)getScaledTouchPoint:(GHOST_WindowIOS *)window;
- (CGPoint)getScaledTranslation:(GHOST_WindowIOS *)window;

- (void)setCachedTranslation:(CGPoint)translation;
- (CGPoint)getCachedTranslation;
@end

@implementation GHOSTUIPanGestureRecognizer

- (CGPoint)getScaledTouchPoint:(GHOST_WindowIOS *)window
{
  CGPoint touch_point = [self locationInView:window->getView()];
  return window->scalePointToWindow(touch_point);
}

- (CGPoint)getScaledTranslation:(GHOST_WindowIOS *)window
{
  CGPoint translation = [self translationInView:window->getView()];
  return window->scalePointToWindow(translation);
}

- (CGPoint)getRelativeTranslation:(CGPoint)translation
{
  CGPoint relative_translation;
  relative_translation.x = translation.x - cached_translation.x;
  relative_translation.y = translation.y - cached_translation.y;
  return relative_translation;
}

- (void)setCachedTranslation:(CGPoint)translation
{
  cached_translation = translation;
}

- (CGPoint)getCachedTranslation
{
  return cached_translation;
}
@end

@interface GHOSTUIHoverGestureRecognizer : UIHoverGestureRecognizer
- (CGPoint)getScaledTouchPoint:(GHOST_WindowIOS *)window;
@end

@implementation GHOSTUIHoverGestureRecognizer

- (CGPoint)getScaledTouchPoint:(GHOST_WindowIOS *)window
{
  CGPoint touch_point = [self locationInView:window->getView()];
  return window->scalePointToWindow(touch_point);
}
@end

@interface GHOSTUIPinchGestureRecognizer : UIPinchGestureRecognizer
{
  CGFloat cached_distance;
}
- (CGPoint)getScaledTouchPoint:(GHOST_WindowIOS *)window touch_id:(int)touch_id;
- (CGFloat)getScaledDistance:(GHOST_WindowIOS *)window;
- (CGPoint)getPinchMidpoint:(GHOST_WindowIOS *)window;
- (void)setCachedDistance:(CGFloat)distance;
- (CGFloat)getCachedDistance;
@end

@implementation GHOSTUIPinchGestureRecognizer
- (CGPoint)getScaledTouchPoint:(GHOST_WindowIOS *)window touch_id:(int)touch_id
{
  CGPoint touch_point = [self locationOfTouch:touch_id inView:window->getView()];
  return window->scalePointToWindow(touch_point);
}

- (CGFloat)getScaledDistance:(GHOST_WindowIOS *)window
{
  CGPoint touch_point0 = [self locationOfTouch:0 inView:window->getView()];
  CGPoint touch_point1 = [self locationOfTouch:1 inView:window->getView()];
  touch_point0 = window->scalePointToWindow(touch_point0);
  touch_point1 = window->scalePointToWindow(touch_point1);
  float dx = touch_point1.x - touch_point0.x;
  float dy = touch_point1.y - touch_point0.y;
  CGFloat point_distance = sqrt(dx * dx + dy * dy);
  return point_distance;
}

- (CGPoint)getPinchMidpoint:(GHOST_WindowIOS *)window
{
  CGPoint touch_point0 = [self locationOfTouch:0 inView:window->getView()];
  CGPoint touch_point1 = [self locationOfTouch:1 inView:window->getView()];
  touch_point0 = window->scalePointToWindow(touch_point0);
  touch_point1 = window->scalePointToWindow(touch_point1);
  CGPoint midPoint = CGPointMake((touch_point0.x + touch_point1.x) / 2.0f,
                                 (touch_point0.y + touch_point1.y) / 2.0f);
  return midPoint;
}

- (void)setCachedDistance:(CGFloat)distance
{
  cached_distance = distance;
}

- (CGFloat)getCachedDistance
{
  return cached_distance;
}
@end

/* GHOSTUIWindow interface. */
@interface GHOSTUIWindow : UIWindow <UIGestureRecognizerDelegate, UIPencilInteractionDelegate>
{
  GHOST_SystemIOS *system;
  GHOST_WindowIOS *window;
  int touch_stack;
  std::unordered_map<uint64_t, TouchData> touchmap;

  GHOSTUITapGestureRecognizer *tap_gesture_recognizer;
  GHOSTUITapGestureRecognizer *tap2f_gesture_recognizer;
  GHOSTUITapGestureRecognizer *tap3f_gesture_recognizer;
  GHOSTUITapGestureRecognizer *tap4f_gesture_recognizer;
  GHOSTUIPanGestureRecognizer *pan_gesture_recognizer;
  GHOSTUIPanGestureRecognizer *pan2f_gesture_recognizer;
  GHOSTUIPinchGestureRecognizer *zoom_gesture_recognizer;
  GHOSTUIHoverGestureRecognizer *hover_gesture_recognizer;
  UIPencilInteraction *pencil_interaction;
  UIScreenEdgePanGestureRecognizer *edge_swipe_left;
  UIScreenEdgePanGestureRecognizer *edge_swipe_right;
  // GHOSTUILongPressGestureRecognizer *long_press_gesture_recognizer;

  /* Data from the Apple pencil */
  UITouch *current_pencil_touch;
  GHOST_TabletData tablet_data;
  bool last_tap_with_pencil;

  /* Keyboard handling. */
  UITextField *text_field;
  NSString *original_text;
  bool onscreen_keyboard_active;
  const char *text_field_string;
  GHOST_KeyboardProperties current_keyboard_properties;
  bool external_keyboard_connected;

  /* Toolbar */
  bool toolbar_enabled;
  UIToolbar *toolbar;
  UIBarButtonItem *toolbar_tip_item;
  UIBarButtonItem *toolbar_live_text_item;
  UIBarButtonItem *toolbar_done_editing_item;
  UIBarButtonItem *toolbar_cancel_editing_item;
}

- (void)setSystemAndWindowIOS:(GHOST_SystemIOS *)sysCocoa windowIOS:(GHOST_WindowIOS *)winCocoa;

/* Blender event generation. */
- (void)generateUserInputEvents:(const UserInputEvent &)event_info;

/* Gesture recognizers. */
- (void)registerGestureRecognizers;
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer *)otherGestureRecognizer;
- (void)handleTap:(GHOSTUITapGestureRecognizer *)sender;
- (void)handlePan:(GHOSTUIPanGestureRecognizer *)sender;
- (void)handlePan2f:(GHOSTUIPanGestureRecognizer *)sender;
- (void)handleZoom:(GHOSTUIPinchGestureRecognizer *)sender;

/* On screen keyboard handling */
- (UITextField *)getUITextField;
- (const GHOST_TabletData)getTabletData;
- (GHOST_TSuccess)popupOnscreenKeyboard:(const GHOST_KeyboardProperties &)keyboard_properties;
- (GHOST_TSuccess)hideOnscreenKeyboard;
- (const char *)getLastKeyboardString;
@end

@implementation GHOSTUIWindow
- (void)setSystemAndWindowIOS:(GHOST_SystemIOS *)sys windowIOS:(GHOST_WindowIOS *)win
{
  system = sys;
  window = win;
  touch_stack = 0;
  text_field = nil;
  original_text = nil;
  onscreen_keyboard_active = false;
  text_field_string = nullptr;
  current_pencil_touch = nil;
  tablet_data = GHOST_TABLET_DATA_NONE;
  toolbar_enabled = true;
  toolbar = nil;
  last_tap_with_pencil = false;
  external_keyboard_connected = [GCKeyboard coalescedKeyboard] != nil;

  /* Register for notifications of chnanges to the onscreen keyboard. */
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(keyboardWillChange:)
                                               name:UIKeyboardWillChangeFrameNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(keyboardWillChange:)
                                               name:UIKeyboardWillShowNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(keyboardWillChange:)
                                               name:UIKeyboardWillHideNotification
                                             object:nil];

  /* Check whether we've linked the GameController framework. */
  if (&GCKeyboardDidConnectNotification != NULL) {
    /* Register for notifcations an external keyboard has been added/removed. */
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(externalKeyboardChange:)
                                                 name:GCKeyboardDidConnectNotification
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(externalKeyboardChange:)
                                                 name:GCKeyboardDidDisconnectNotification
                                               object:nil];
  }
}

- (void)registerGestureRecognizers
{
  /** Create Gesture recognisers. */
  /* Tap gesture recognizer. */
  tap_gesture_recognizer = [[GHOSTUITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleTap:)];
  tap_gesture_recognizer.delegate = self;
  tap_gesture_recognizer.cancelsTouchesInView = false;
  tap_gesture_recognizer.allowedTouchTypes = @[ @(UITouchTypePencil), @(UITouchTypeDirect) ];
  [window->getView() addGestureRecognizer:tap_gesture_recognizer];

  /* Two-finger tap gesture recognizer. */
  tap2f_gesture_recognizer = [[GHOSTUITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleTap2F:)];
  tap2f_gesture_recognizer.delegate = self;
  tap2f_gesture_recognizer.cancelsTouchesInView = false;
  tap2f_gesture_recognizer.delaysTouchesBegan = YES;
  tap2f_gesture_recognizer.numberOfTouchesRequired = 2;
  [window->getView() addGestureRecognizer:tap2f_gesture_recognizer];

  /* Three-finger tap gesture recognizer. */
  tap3f_gesture_recognizer = [[GHOSTUITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleTap3F:)];
  tap3f_gesture_recognizer.delegate = self;
  tap3f_gesture_recognizer.cancelsTouchesInView = false;
  tap3f_gesture_recognizer.delaysTouchesBegan = YES;
  tap3f_gesture_recognizer.numberOfTouchesRequired = 3;
  [window->getView() addGestureRecognizer:tap3f_gesture_recognizer];

  /* Four-finger tap gesture recognizer. */
  tap4f_gesture_recognizer = [[GHOSTUITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleTap4F:)];
  tap4f_gesture_recognizer.delegate = self;
  tap4f_gesture_recognizer.cancelsTouchesInView = false;
  tap4f_gesture_recognizer.delaysTouchesBegan = YES;
  tap4f_gesture_recognizer.numberOfTouchesRequired = 4;
  [window->getView() addGestureRecognizer:tap4f_gesture_recognizer];

  /* Pan gesture recognizer - static UI. */
  pan_gesture_recognizer = [[GHOSTUIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePan:)];
  pan_gesture_recognizer.delegate = self;
  pan_gesture_recognizer.cancelsTouchesInView = false;
  /* Allow scrolling only with a single finger. */
  pan_gesture_recognizer.minimumNumberOfTouches = 1;
  pan_gesture_recognizer.maximumNumberOfTouches = 1;
  /* Allow finger and pencil. */
  pan_gesture_recognizer.allowedTouchTypes = @[ @(UITouchTypePencil), @(UITouchTypeDirect) ];
  [window->getView() addGestureRecognizer:pan_gesture_recognizer];

  /* Pan gesture recognizer - two fingers 3D UI. */
  pan2f_gesture_recognizer = [[GHOSTUIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePan2f:)];
  pan2f_gesture_recognizer.delegate = self;
  pan2f_gesture_recognizer.cancelsTouchesInView = false;
  /* Two finger gestures only.  */
  pan2f_gesture_recognizer.minimumNumberOfTouches = 2;
  pan2f_gesture_recognizer.maximumNumberOfTouches = 2;
  [window->getView() addGestureRecognizer:pan2f_gesture_recognizer];

  /* Pinch/Zoom gesture recognizer. */
  zoom_gesture_recognizer = [[GHOSTUIPinchGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleZoom:)];
  zoom_gesture_recognizer.delegate = self;
  zoom_gesture_recognizer.cancelsTouchesInView = false;
  [window->getView() addGestureRecognizer:zoom_gesture_recognizer];

  /* Edge swipe. */
  edge_swipe_left = [[UIScreenEdgePanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleEdgeSwipe:)];
  edge_swipe_left.edges = UIRectEdgeLeft;
  edge_swipe_left.delegate = self;
  [window->getView() addGestureRecognizer:edge_swipe_left];

  edge_swipe_right = [[UIScreenEdgePanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleEdgeSwipe:)];
  edge_swipe_right.edges = UIRectEdgeRight;
  edge_swipe_right.delegate = self;
  [window->getView() addGestureRecognizer:edge_swipe_right];

  /* Apple Pencil hover recognizer. */
  hover_gesture_recognizer = [[GHOSTUIHoverGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleHover:)];
  hover_gesture_recognizer.delegate = self;
  [window->getView() addGestureRecognizer:hover_gesture_recognizer];
  current_pencil_touch = nil;

  /**  Apple Pencil double-tap. */
  pencil_interaction = [[UIPencilInteraction alloc] init];
  pencil_interaction.delegate = self;
  [window->getView() addInteraction:pencil_interaction];
}

/* Turn the user inputs into Blender events.
 * We batch up the events rather than send them directly in the gesture
 * recognisers to ensure we don't interleave events if we detect simultaneous
 * inputs. */
- (void)generateUserInputEvents:(const UserInputEvent &)event_info
{
  /* Lock access to ensure all input-events are received sequentially. */
  @synchronized(self) {
    for (int i = 0; i < event_info.num_events; i++) {
      UserInputEvent::EventTypes event_type = event_info.event_list[i];
      IOS_INPUT_LOG(@"%d-%@ %f,%f",
                    i,
                    event_info.getEventTypeDesc(event_type),
                    event_info.location.x,
                    event_info.location.y);

      switch (event_type) {
        case UserInputEvent::EventTypes::CURSOR_MOVE:
          system->pushEvent(
              new GHOST_EventCursor(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                    GHOST_kEventCursorMove,
                                    window,
                                    event_info.location.x,
                                    event_info.location.y,
                                    tablet_data));
          break;
        case UserInputEvent::EventTypes::PAN_GESTURE:
          system->pushEvent(
              new GHOST_EventTrackpad(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                      window,
                                      GHOST_kTrackpadEventScroll,
                                      event_info.location.x,
                                      event_info.location.y,
                                      event_info.translation.x,
                                      event_info.translation.y,
                                      false,
                                      1));
          break;
        case UserInputEvent::EventTypes::PAN_GESTURE_TWO_FINGERS:
          system->pushEvent(
              new GHOST_EventTrackpad(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                      window,
                                      GHOST_kTrackpadEventScroll,
                                      event_info.location.x,
                                      event_info.location.y,
                                      event_info.translation.x,
                                      event_info.translation.y,
                                      true,
                                      2));
          break;
        case UserInputEvent::EventTypes::LEFT_BUTTON_DOWN:
          system->pushEvent(
              new GHOST_EventButton(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                    GHOST_kEventButtonDown,
                                    window,
                                    GHOST_kButtonMaskLeft,
                                    tablet_data));
          break;
        case UserInputEvent::EventTypes::LEFT_BUTTON_UP:
          system->pushEvent(
              new GHOST_EventButton(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                    GHOST_kEventButtonUp,
                                    window,
                                    GHOST_kButtonMaskLeft,
                                    tablet_data));
          break;
        case UserInputEvent::EventTypes::PINCH_GESTURE:
          system->pushEvent(
              new GHOST_EventTrackpad(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                      window,
                                      GHOST_kTrackpadEventMagnify,
                                      event_info.location.x,
                                      event_info.location.y,
                                      event_info.distance,
                                      0,
                                      false,
                                      2));
          break;
        case UserInputEvent::EventTypes::PENCIL_TAP:
          /* Simulate clicking with the right mouse button. */
          system->pushEvent(
              new GHOST_EventButton(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                    GHOST_kEventButtonDown,
                                    window,
                                    GHOST_kButtonMaskRight,
                                    tablet_data));
          break;
        default:
          GHOST_ASSERT(FALSE, "GHOST_SystemIOS::generateUserInputEvents unsupported event type");
      }
    }
  }
}

/* Allow simultaneous gestures for two finger pans and zooms but nothing else. */
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer *)otherGestureRecognizer
{
  if (gestureRecognizer == pan2f_gesture_recognizer &&
      otherGestureRecognizer == zoom_gesture_recognizer)
  {
    return YES;
  }
  if (gestureRecognizer == pan_gesture_recognizer &&
      otherGestureRecognizer == zoom_gesture_recognizer)
  {
    return YES;
  }
  return NO;
}

/* Override touch methods to capture the UITouch object. */
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesBegan:touches withEvent:event];

  for (UITouch *touch in touches) {
    if (touch.type == UITouchTypePencil) {
      current_pencil_touch = touch;
      break;
    }
  }
}

/* Get updated tablet data. */
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesMoved:touches withEvent:event];

  /* Check if one of the touches was from a pencil. */
  if (current_pencil_touch) {
    /* Iterate through all pencil touches. */
    for (UITouch *touch in touches) {
      if (touch.type == UITouchTypePencil) {
        current_pencil_touch = touch;

        tablet_data.Active = GHOST_kTabletModeStylus;

        /* Map apple pessure Range to Blender range: 0.0 (not touching) to 1.0 (full pressure). */
        tablet_data.Pressure = current_pencil_touch.force /
                               current_pencil_touch.maximumPossibleForce;

        CGFloat azimuthAngle = [current_pencil_touch azimuthAngleInView:window->getView()];
        CGFloat altitudeAngle = [current_pencil_touch altitudeAngle];

        /* Calculate the maximum possible tilt (1.0) when altitude is 0. */
        CGFloat maxTilt = cos(0);

        /* Convert to x and y tilt - range -1.0 (left) to +1.0 (right). */
        tablet_data.Xtilt = sin(azimuthAngle) * cos(altitudeAngle) / maxTilt;
        tablet_data.Ytilt = -cos(azimuthAngle) * cos(altitudeAngle) / maxTilt;
        IOS_INPUT_LOG(
            @"TABLET: X:%f,Y:%f,P:%f", tablet_data.Xtilt, tablet_data.Ytilt, tablet_data.Pressure);
        break;
      }
    }
  }
}

/* Reset tablet data. */
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesEnded:touches withEvent:event];
  current_pencil_touch = nil;
  tablet_data = GHOST_TABLET_DATA_NONE;
}

/* Reset tablet data. */
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [super touchesCancelled:touches withEvent:event];
  current_pencil_touch = nil;
  tablet_data = GHOST_TABLET_DATA_NONE;
}

- (void)handleTap:(GHOSTUITapGestureRecognizer *)sender
{
  CGPoint touch_point = [sender getScaledTouchPoint:window];
  last_tap_with_pencil = current_pencil_touch ? true : false;
  UserInputEvent event_info(&touch_point, nullptr, nullptr, last_tap_with_pencil);

  /* Send events to indicate a 'click' on event end. */
  if (sender.state == UIGestureRecognizerStateEnded) {
    event_info.add_event(UserInputEvent::EventTypes::CURSOR_MOVE);
    event_info.add_event(UserInputEvent::EventTypes::LEFT_BUTTON_DOWN);
    event_info.add_event(UserInputEvent::EventTypes::LEFT_BUTTON_UP);
  }

  [self generateUserInputEvents:event_info];
}

- (void)handleTap2F:(GHOSTUITapGestureRecognizer *)sender
{
  if (sender.state != UIGestureRecognizerStateEnded) {
    return;
  }

  CGPoint touch_point = [sender locationInView:window->getView()];
  CGFloat scale = [window->getView() contentScaleFactor];
  touch_point.x *= scale;
  touch_point.y *= scale;

  system->pushEvent(new GHOST_Event(
      GHOST_GetMilliSeconds((GHOST_SystemHandle)system), GHOST_kEventTwoFingerTap, window));
}

- (void)handleTap3F:(GHOSTUITapGestureRecognizer *)sender
{
  if (sender.state != UIGestureRecognizerStateEnded) {
    return;
  }

  CGPoint touch_point = [sender locationInView:window->getView()];
  CGFloat scale = [window->getView() contentScaleFactor];
  touch_point.x *= scale;
  touch_point.y *= scale;

  system->pushEvent(new GHOST_Event(
      GHOST_GetMilliSeconds((GHOST_SystemHandle)system), GHOST_kEventThreeFingerTap, window));
}

- (void)handleTap4F:(GHOSTUITapGestureRecognizer *)sender
{
  if (sender.state != UIGestureRecognizerStateEnded) {
    return;
  }

  CGPoint touch_point = [sender locationInView:window->getView()];
  CGFloat scale = [window->getView() contentScaleFactor];
  touch_point.x *= scale;
  touch_point.y *= scale;

  system->pushEvent(new GHOST_Event(
      GHOST_GetMilliSeconds((GHOST_SystemHandle)system), GHOST_kEventFourFingerTap, window));
}

- (void)handlePan:(GHOSTUIPanGestureRecognizer *)sender
{
  CGPoint touch_point = [sender getScaledTouchPoint:window];
  CGPoint translation = [sender getScaledTranslation:window];
  bool pencil_pan = current_pencil_touch ? true : false;

  UserInputEvent event_info(&touch_point, nullptr, nullptr, pencil_pan);

  if (sender.state == UIGestureRecognizerStateBegan ||
      sender.state == UIGestureRecognizerStateChanged)
  {
    /* Register initial click for click and drag support. */
    if (sender.state == UIGestureRecognizerStateBegan) {
      /* Set inital translation */
      [sender setCachedTranslation:translation];
      event_info.add_event(UserInputEvent::EventTypes::CURSOR_MOVE);
      event_info.add_event(UserInputEvent::EventTypes::LEFT_BUTTON_DOWN);
    }

    /* Calculate translation change since last begin/change event */
    CGPoint relative_translation = [sender getRelativeTranslation:translation];
    /* Update cached translation */
    [sender setCachedTranslation:translation];
    /* Send pan event if non zero */
    if (!CGPointEqualToPoint(relative_translation, CGPointMake(0.0f, 0.0f))) {
      event_info.translation = relative_translation;
      event_info.add_event(UserInputEvent::EventTypes::PAN_GESTURE);
    }

    /* Update cursor position on change */
    if (sender.state == UIGestureRecognizerStateChanged) {
      event_info.add_event(UserInputEvent::EventTypes::CURSOR_MOVE);
    }
  }

  /* Mouse release for pan. */
  if (sender.state == UIGestureRecognizerStateEnded ||
      sender.state == UIGestureRecognizerStateCancelled ||
      sender.state == UIGestureRecognizerStateFailed)
  {
    event_info.add_event(UserInputEvent::EventTypes::LEFT_BUTTON_UP);
  }
  [self generateUserInputEvents:event_info];
}

- (void)handlePan2f:(GHOSTUIPanGestureRecognizer *)sender
{
  /* Translation can be non-zero on begin event */
  if (sender.state == UIGestureRecognizerStateBegan ||
      sender.state == UIGestureRecognizerStateChanged)
  {
    CGPoint translation = [sender getScaledTranslation:window];

    /* Calculate translation relative to previous cached value. */
    CGPoint relative_translation = [sender getRelativeTranslation:translation];

    /* Cache new translation. */
    [sender setCachedTranslation:translation];

    /* Generate pan event if translation is non zero. */
    if (!CGPointEqualToPoint(relative_translation, CGPointMake(0.0f, 0.0f))) {
      CGPoint touch_point = [sender getScaledTouchPoint:window];
      bool pencil_pan = current_pencil_touch ? true : false;
      UserInputEvent event_info(&touch_point, &relative_translation, nullptr, pencil_pan);
      event_info.add_event(UserInputEvent::EventTypes::PAN_GESTURE_TWO_FINGERS);
      [self generateUserInputEvents:event_info];
    }
  }
  else if (sender.state == UIGestureRecognizerStateEnded ||
           sender.state == UIGestureRecognizerStateCancelled ||
           sender.state == UIGestureRecognizerStateFailed)
  {
    /* Set translation back to zero. */
    [sender setCachedTranslation:CGPointMake(0.0f, 0.0f)];
  }
}

- (void)handleEdgeSwipe:(UIScreenEdgePanGestureRecognizer *)gesture
{
  if (gesture.state != UIGestureRecognizerStateEnded) {
    return;
  }

  UIView *view = window->getView();
  CGPoint location = [gesture locationInView:view];
  CGSize viewSize = view.bounds.size;

  GHOST_TTouchEventSubTypes ghostEventType;

  if (gesture.edges == UIRectEdgeLeft) {
    ghostEventType = GHOST_kTouchEventEdgeSwipeInLeft;
  }
  else if (gesture.edges == UIRectEdgeRight) {
    ghostEventType = GHOST_kTouchEventEdgeSwipeInRight;
  }
  else {
    /* For now only handle left/right. */
    return;
  }

  system->pushEvent(new GHOST_EventTouch(
      system->getMilliSeconds(), window, ghostEventType, location.x, location.y));
}

- (void)handleHover:(GHOSTUIHoverGestureRecognizer *)sender
{
  if (sender.state == UIGestureRecognizerStateBegan ||
      sender.state == UIGestureRecognizerStateChanged)
  {
    /* Tablet needs to be set to stylus mode because we need
     * wmTabletData.is_motion_absolute set to true. */
    tablet_data.Active = GHOST_kTabletModeStylus;
    CGPoint hover_point = [sender getScaledTouchPoint:window];
    /* Add cursor move event. */
    UserInputEvent event_info(&hover_point, nullptr, nullptr, true);
    event_info.add_event(UserInputEvent::EventTypes::CURSOR_MOVE);
    [self generateUserInputEvents:event_info];
  }
  else if (sender.state == UIGestureRecognizerStateEnded ||
           sender.state == UIGestureRecognizerStateCancelled ||
           sender.state == UIGestureRecognizerStateFailed)
  {
    tablet_data = GHOST_TABLET_DATA_NONE;
  }
}

- (void)handleZoom:(GHOSTUIPinchGestureRecognizer *)sender
{
  /* Ignore any calls where don't have two touches to work with. */
  if ([sender numberOfTouches] < 2) {
    return;
  }

  /* Pinch/Zoom gestures */
  if (sender.state == UIGestureRecognizerStateBegan) {
    /* Set an initial distance value. */
    CGFloat point_distance = [sender getScaledDistance:window];
    [sender setCachedDistance:point_distance];
  }
  else if (sender.state == UIGestureRecognizerStateChanged) {

    /* Calculate change in distance since last event */
    CGFloat point_distance = [sender getScaledDistance:window];
    CGFloat relative_dist = point_distance - [sender getCachedDistance];

    /* Updated cached distance. */
    [sender setCachedDistance:point_distance];

    /* Send pinch/zoom event. */
    if (fabs(relative_dist) > 0.0) {
      /* Calculate midpoint between the two touch points. */
      CGPoint midPoint = [sender getPinchMidpoint:window];

      UserInputEvent event_info(&midPoint, nullptr, &relative_dist, false);
      event_info.add_event(UserInputEvent::EventTypes::PINCH_GESTURE);
      [self generateUserInputEvents:event_info];
    }
  }
  /* Nothing to do here. */
  else if (sender.state == UIGestureRecognizerStateEnded ||
           sender.state == UIGestureRecognizerStateCancelled ||
           sender.state == UIGestureRecognizerStateFailed)
  {
  }
}

- (void)pencilInteractionDidTap:(UIPencilInteraction *)interaction
{
  UserInputEvent event_info(nullptr, nullptr, nullptr, true);
  event_info.add_event(UserInputEvent::EventTypes::PENCIL_TAP);
  [self generateUserInputEvents:event_info];
}

- (void)beginFrame
{
}

- (void)endFrame
{
}

- (void)initToolbar
{
  /* This gets the current view size */
  UIView *ui_view = window->getView();
  CGSize frame_size = [ui_view sizeThatFits:CGSizeMake(0.0f, 0.0f)];
  /* Create a toolbar the width of the screen. */
  toolbar = [[UIToolbar alloc] initWithFrame:CGRectMake(0, 0, frame_size.width, 44)];
  toolbar.barStyle = UIBarStyleDefault;
  toolbar.translucent = true;
  /* IOS_FIXME - Despite following Apple guidelines this toolbar still
   * appears to apparently violate the view constraints. It displays fine
   * but generates a lot of warning output to the console. */
  toolbar.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [toolbar sizeToFit];

  toolbar_tip_item = [[UIBarButtonItem alloc] initWithTitle:@""
                                                      style:UIBarButtonItemStylePlain
                                                     target:nil
                                                     action:nil];

  toolbar_live_text_item = [[UIBarButtonItem alloc] initWithTitle:@""
                                                            style:UIBarButtonItemStylePlain
                                                           target:nil
                                                           action:nil];

  toolbar_done_editing_item = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:nil
                           action:@selector(handleDoneButton)];

  toolbar_cancel_editing_item = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:nil
                           action:@selector(handleCancelButton)];

  /* Prevents editing of tip and live text fields. */
  toolbar_tip_item.enabled = NO;
  toolbar_live_text_item.enabled = NO;
  toolbar_live_text_item.tintColor = UIColor.blackColor;

  /* Set the live text to a fixed width. */
  /* IOS_FIXME - should this be set dynamically? Need to move out of init if so. */
  toolbar_live_text_item.width = 150.0f;

  toolbar.items = @[
    toolbar_tip_item,
    toolbar_live_text_item,
    toolbar_done_editing_item,
    toolbar_cancel_editing_item
  ];
}

- (void)generateKeyboardReturnEvent
{
  /*
   Only push the event back if the keyboard is active otherwise we may generate new
   spurious events.
   */
  if (onscreen_keyboard_active) {
    /*
     This event should cause ui_textedit_end() to be called which will
     hide the keyboard.
     */
    system->pushEvent(new GHOST_EventKey(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                         GHOST_kEventKeyDown,
                                         window,
                                         GHOST_kKeyEnter,
                                         false,
                                         nullptr));
  }
  else {
    IOS_INPUT_LOG(@"Ignoring handleKeyboardReturn %@", text_field.text);
  }
}

- (void)handleKeyboardReturn:(UITextField *)text_field
{
  @synchronized(self) {
    IOS_INPUT_LOG(@"handleKeyboardReturn %@", text_field.text);
    [self generateKeyboardReturnEvent];
  }
}

- (void)handleKeyboardEditChange:(UITextField *)text_field
{
  @synchronized(self) {

    /* Update the text in the tool bar as the edits arrive. */
    if (toolbar_live_text_item) {
      toolbar_live_text_item.title = text_field.text;
      /* Force toolbar to update */
      [toolbar setNeedsLayout];
      [toolbar layoutIfNeeded];
    }
    IOS_INPUT_LOG(@"Keyboard Edit change detected %@", text_field.text);

    /* IOS_FIXME - Enabling this will propogate text changes back into the Blender text field
     as they happen. Since pushing back individual key presses appears to be difficult this
     might be the best we can do. However this currently causes a segmentation fault if you delete
     text as the Blender-side string ends up being NULL in some cases. */
    bool push_edits_back_to_blender = false;

    if (push_edits_back_to_blender) {
      system->pushEvent(new GHOST_EventKey(GHOST_GetMilliSeconds((GHOST_SystemHandle)system),
                                           GHOST_kEventKeyDown,
                                           window,
                                           GHOST_kKeyTextEdit,
                                           false,
                                           nullptr));
    }
  }
}

- (void)handleKeyboardEditBegin:(UITextField *)text_field
{
  @synchronized(self) {
    IOS_INPUT_LOG(@"Keyboard Edit begin detected %@", text_field.text);
  }
}

- (void)handleKeyboardEditEnd:(UITextField *)text_field
{
  @synchronized(self) {
    /*
     This can get called when the keyboard is minimised
     so send a return keypress to emulate effective end
     of editing. Otherwise Blender's focus will remain
     on the text field.
     */
    IOS_INPUT_LOG(@"Keyboard Edit end detected %@", text_field.text);
    [self generateKeyboardReturnEvent];
  }
}

- (void)handleDoneButton
{
  IOS_INPUT_LOG(@"Keyboard Done button press detected %@", text_field.text);
  [self generateKeyboardReturnEvent];
}

- (void)handleCancelButton
{
  IOS_INPUT_LOG(@"Keyboard Cancel button press detected %@", text_field.text);
  /* Restore the original text and return */
  text_field.text = original_text;
  [self generateKeyboardReturnEvent];
}

/*
 * Add a text field so we can handle input from a popup keyboard and
 * attach it to our root window.
 */
- (void)initUITextField
{
  /* Initialise it if we have not already done so. */
  if (!text_field) {
    text_field = [[UITextField alloc] init];

    text_field.contentScaleFactor = window->getWindowScaleFactor();

    if (toolbar_enabled) {
      [self initToolbar];
      text_field.inputAccessoryView = toolbar;
    }

    [window->rootWindow addSubview:text_field];

    /* Add a handler for when 'return' is pressed on keyboard. */
    [text_field addTarget:self
                   action:@selector(handleKeyboardReturn:)
         forControlEvents:UIControlEventEditingDidEndOnExit];

    /* Add a handler for when the text field changes. */
    [text_field addTarget:self
                   action:@selector(handleKeyboardEditChange:)
         forControlEvents:UIControlEventEditingChanged];

    /* Add a handler for when user edits a text field. */
    [text_field addTarget:self
                   action:@selector(handleKeyboardEditBegin:)
         forControlEvents:UIControlEventEditingDidBegin];

    /* Add a handler for when user finishes editing a text field. */
    [text_field addTarget:self
                   action:@selector(handleKeyboardEditEnd:)
         forControlEvents:UIControlEventEditingDidEnd];
  }
}

- (void)convertWindowCoordToDisplayCoordWithWindow:(int)windowX
                                           windowY:(int)windowY
                                          displayX:(double *)displayX
                                          displayY:(double *)displayY
                                             flipY:(BOOL)flipY
{
  float pixelScale = window->getWindowScaleFactor();
  CGSize logicalWindowSize = window->getLogicalWindowSize();

  *displayX = (double)windowX / pixelScale;
  *displayY = (double)windowY / pixelScale;

  if (flipY) {
    *displayY = logicalWindowSize.height - *displayY;
  }
}

- (UITextField *)getUITextField
{
  return text_field;
}

- (void)setupKeyboard:(const GHOST_KeyboardProperties &)keyboard_properties
{
  /* Initialise it if we have not already done so */
  if (!text_field) {
    [self initUITextField];
  }

  /* Save this set of keyboard properties */
  current_keyboard_properties = keyboard_properties;

  /* Convert the text box coords to display coords */
  CGRect displayRect;
  [self convertWindowCoordToDisplayCoordWithWindow:keyboard_properties.text_box_origin[0]
                                           windowY:keyboard_properties.text_box_origin[1]
                                          displayX:&displayRect.origin.x
                                          displayY:&displayRect.origin.y
                                             flipY:true];

  [self convertWindowCoordToDisplayCoordWithWindow:keyboard_properties.text_box_size[0]
                                           windowY:keyboard_properties.text_box_size[1]
                                          displayX:&displayRect.size.width
                                          displayY:&displayRect.size.height
                                             flipY:false];

  /* Where to display the text on-screen. */
  text_field.frame = displayRect;

  /* Initialise text with existing string. */
  text_field.text = keyboard_properties.text_string ?
                        [NSString stringWithUTF8String:keyboard_properties.text_string] :
                        @"";
  /* Take a copy of the string so we can restore it if neccessary */
  original_text = keyboard_properties.text_string ?
                      [NSString stringWithUTF8String:keyboard_properties.text_string] :
                      @"";

  /* Set keyboard type and text alignment.
   * NOTE - the keyboard type is only honoured if using an Apple
   * pencil or if the keyboard is floating.
   * Otherwise it will just be the default full screen type. */
  switch (keyboard_properties.keyboard_type) {
    case GHOST_KeyboardProperties::ascii_keyboard_type: {
      text_field.keyboardType = UIKeyboardTypeASCIICapable;
      text_field.textAlignment = NSTextAlignmentLeft;
      break;
    }
    case GHOST_KeyboardProperties::decimal_numpad_keyboard_type: {
      text_field.keyboardType = UIKeyboardTypeDecimalPad;
      text_field.textAlignment = NSTextAlignmentCenter;
      break;
    }
    case GHOST_KeyboardProperties::numpad_keyboard_type: {
      text_field.keyboardType = UIKeyboardTypeNumberPad;
      text_field.textAlignment = NSTextAlignmentCenter;
      break;
    }
    default: {
      /* What's the sensible baviour here? Default? Assert? */
      text_field.keyboardType = UIKeyboardTypeDefault;
      text_field.textAlignment = NSTextAlignmentLeft;
    }
  }
  /* Reset keyboard type to default if not using Apple Pencil
   * or it's not floating. (Need to add floating detection.) */
  if (!last_tap_with_pencil) {
    // text_field.keyboardType = UIKeyboardTypeDefault;
  }

  /* Set light/dark mode or adopt system default. */
  text_field.keyboardAppearance = UIKeyboardAppearanceDefault;

  /* This seems sensible given Blender's typical behaviour. */
  text_field.autocorrectionType = UITextAutocorrectionTypeNo;
  text_field.spellCheckingType = UITextSpellCheckingTypeNo;

  /* Set font size. */
  float fontSize = keyboard_properties.font_size / window->getWindowScaleFactor();
  text_field.font = [UIFont systemFontOfSize:fontSize];

  /* Set font color. */
  text_field.textColor = [UIColor colorWithRed:keyboard_properties.font_color[0]
                                         green:keyboard_properties.font_color[1]
                                          blue:keyboard_properties.font_color[2]
                                         alpha:keyboard_properties.font_color[3]];

  /* Initial highlighting and text-cursor position. */
  switch (keyboard_properties.inital_text_state) {
    case GHOST_KeyboardProperties::select_all_text: {
      [text_field selectAll:nil];
      break;
    }
    case GHOST_KeyboardProperties::select_text_range: {
      UITextPosition *startPosition = [text_field
          positionFromPosition:text_field.beginningOfDocument
                        offset:keyboard_properties.text_select_range[0]];
      UITextPosition *endPosition = [text_field
          positionFromPosition:text_field.beginningOfDocument
                        offset:keyboard_properties.text_select_range[1]];
      text_field.selectedTextRange = [text_field textRangeFromPosition:startPosition
                                                            toPosition:endPosition];
      break;
    }
    case GHOST_KeyboardProperties::move_cursor_to_start: {
      UITextPosition *beginning = text_field.beginningOfDocument;
      text_field.selectedTextRange = [text_field textRangeFromPosition:beginning
                                                            toPosition:beginning];
      break;
    }
    case GHOST_KeyboardProperties::move_cursor_to_end: {
      UITextPosition *end = text_field.endOfDocument;
      text_field.selectedTextRange = [text_field textRangeFromPosition:end toPosition:end];
      break;
    }
    default: {
      GHOST_ASSERT(FALSE, "GHOST_SystemIOS::setupTextField unsupported text select option");
    }
  }

  /* Setup the tool bar if it's enabled. */
  if (toolbar_enabled) {
    toolbar_live_text_item.title = text_field.text;
    toolbar_tip_item.title = keyboard_properties.tip_text ?
                                 [NSString stringWithCString:keyboard_properties.tip_text
                                                    encoding:NSUTF8StringEncoding] :
                                 @"";
  }
}

- (void)externalKeyboardChange:(NSNotification *)notification
{
  external_keyboard_connected = [GCKeyboard coalescedKeyboard] != nil;
  IOS_INPUT_LOG(@"External Keyboard %s",
                external_keyboard_connected ? "Connected" : "Disconnected");
}

/* IOS_FIXME - Not currently used, could be removed. */
- (void)keyboardWillChange:(NSNotification *)notification
{

  CGRect keyboardRect = [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  /* Sometimes we see a zero value for the end-frame value, possibly because... timing? */
  if (keyboardRect.size.width == 0 || keyboardRect.size.height == 0) {
    keyboardRect = [notification.userInfo[UIKeyboardFrameBeginUserInfoKey] CGRectValue];
  }
}

- (const GHOST_TabletData)getTabletData
{
  return tablet_data;
}

- (GHOST_TSuccess)popupOnscreenKeyboard:(const GHOST_KeyboardProperties &)keyboard_properties
{
  @synchronized(self) {
    IOS_INPUT_LOG(@"Keyboard popup request received %@", text_field.text);
    [self setupKeyboard:keyboard_properties];

    if (!onscreen_keyboard_active) {
      text_field.userInteractionEnabled = YES;
      if (![text_field becomeFirstResponder]) {
        GHOST_ASSERT(FALSE, "GHOST_SystemIOS::popupOnScreenKeyboard Failed to display keyboard");
      }
      onscreen_keyboard_active = true;
    }
  }
  return GHOST_kSuccess;
}

- (GHOST_TSuccess)hideOnscreenKeyboard
{
  /* Lock access around keyboard handling events. */
  @synchronized(self) {
    IOS_INPUT_LOG(@"Keyboard hide request received %@", text_field.text);

    if (onscreen_keyboard_active) {
      /*
       This must come first so that any of the keyboard event handlers that get
       triggered in response to shutting down the keyboard don't do anything
       (like generating events back to Blender)
       */
      onscreen_keyboard_active = false;

      /* Shut down the keyboard. */
      [text_field resignFirstResponder];
      /*
       IOS_FIXME - Note: This may cause the console to display the warning message:
       "-[UIApplication _touchesEvent] will no longer work as expected. Please stop using it."
       But since this is being generated by Apple OS code there's nothing obvious to fix it right
       now.
       */

      IOS_INPUT_LOG(@"Resigned keyboard responder");
      /*
       This is required to disable any subsequent interactions with the text field that could
       potentially bypass Blender's input handling (since the UITextField is now live
       on the view)
       */
      text_field.userInteractionEnabled = NO;

      /* Save the input to a c-string */
      text_field_string = [[text_field text] UTF8String];

      /* Delete the text field copy of the string */
      text_field.text = nil;
    }
  }
  IOS_INPUT_LOG(@"Text field value was %s", text_field_string);
  return GHOST_kSuccess;
}

- (const char *)getLastKeyboardString
{
  /* Lock access around keyboard handling events */
  @synchronized(self) {

    /* Update text string if one exists */
    if (text_field.text && ![text_field.text isEqualToString:@""]) {
      /* Save the input to a c-string */
      text_field_string = [[text_field text] UTF8String];
    }
  }
  return text_field_string;
}

@end

@interface GHOST_IOSViewController : UIViewController

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)mtkView;

@end

@implementation GHOST_IOSViewController
{
  MTKView *_view;
  GHOST_IOSMetalRenderer *_renderer;
}

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)mtkView
{
  _view = mtkView;
  _view.multipleTouchEnabled = YES;
  self = [super init];
  self.view = (UIView *)mtkView;

  return self;
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  _view = (MTKView *)self.view;
  _view.enableSetNeedsDisplay = NO;
  _view.device = MTLCreateSystemDefaultDevice();
  _view.clearColor = MTLClearColorMake(0, 0, 0, 1.0);
  _view.paused = NO;
  _view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
  _view.autoResizeDrawable = YES;
  _view.contentMode = UIViewContentModeScaleToFill;
  _view.contentScaleFactor = [[UIScreen mainScreen] scale];
  /* Set the refresh rate to the screen's maximum. There may be some value in capping
   * this value to preserve battery life (60fps seems to work well). */
  _view.preferredFramesPerSecond = [UIScreen mainScreen].maximumFramesPerSecond;
  _renderer = [[GHOST_IOSMetalRenderer alloc] initWithMetalKitView:_view];
  if (!_renderer) {
    NSLog(@"Renderer initialization failed");
    return;
  }

  [_renderer mtkView:_view drawableSizeWillChange:_view.drawableSize];

  _view.delegate = _renderer;
}

- (void)handleGesture:(UIGestureRecognizer *)gestureRecognizer
{
}

- (BOOL)prefersHomeIndicatorAutoHidden
{
  /* Make the Home Indicator (the bottom-center white navigation bar) auto-hide when possible. */
  return YES;
}

@end

GHOST_WindowIOS::GHOST_WindowIOS(GHOST_SystemIOS *system_ios,
                                 const char *title,
                                 int32_t left,
                                 int32_t bottom,
                                 uint32_t width,
                                 uint32_t height,
                                 GHOST_TWindowState state,
                                 GHOST_TDrawingContextType type,
                                 const GHOST_ContextParams &context_params,
                                 bool /*is_dialog*/,
                                 GHOST_WindowIOS *parent_window)
    : GHOST_Window(width, height, state, context_params, false), metal_view_(nil)
{
  full_screen_ = false;
  system_ios_ = system_ios;
  /* Parent window will be the window that focus is returned to upon close. */
  parent_window_ = parent_window;
  window_title_ = nullptr;

  /* Create MTKView. */
  metal_view_ = [[MTKView alloc] initWithFrame:CGRectMake(left, bottom, width, height)];
  [metal_view_ retain];
  GHOST_ASSERT(metal_view_, "metalview not valid");

  /* Create view controller. */
  UIApplication *app = [UIApplication sharedApplication];
  GHOST_ASSERT(app, "App not valid");
  id<UIApplicationDelegate> app_delegate = [app delegate];
  GHOST_ASSERT(app_delegate, "App not valid");

  GHOSTUIWindow *ghost_rootWindow = nullptr;

  if (full_screen_) {
    /* Init window at native res. */
    ghost_rootWindow = [[GHOSTUIWindow alloc] init];
    [ghost_rootWindow retain];
    /* Ensure fullscreen. */
    CGRect rect = [UIScreen mainScreen].bounds;
    rootWindow.frame = rect;
  }
  else {
    /* Init window at specified size. */
    ghost_rootWindow = [[GHOSTUIWindow alloc]
        initWithFrame:CGRectMake(left, bottom, width, height)];
    [ghost_rootWindow retain];
    [ghost_rootWindow setClipsToBounds:YES];
  }

  rootWindow = (UIWindow *)ghost_rootWindow;

  [ghost_rootWindow setSystemAndWindowIOS:system_ios_ windowIOS:this];
  rootWindow.windowLevel = UIWindowLevelAlert;

  GHOST_ASSERT(rootWindow, "UIWindow not valid");
  uiview_controller_ = [[[GHOST_IOSViewController alloc] initWithMetalKitView:metal_view_]
      retain];
  [uiview_controller_ viewDidLoad];
  GHOST_ASSERT(uiview_controller_, "UIViewController not valid");

  /* Set presentation style depending on whether main window, dialog or temporary window. */
  if (full_screen_) {
    /* Initial window has no parent and is always fullscreen. */
    uiview_controller_.modalPresentationStyle = UIModalPresentationFullScreen;
  }
  else {
    /* Initial window has no parent and is always fullscreen. */
    uiview_controller_.modalPresentationStyle = UIModalPresentationPageSheet;
  }
  rootWindow.rootViewController = uiview_controller_;

  /* Create UIView */
  GHOST_ASSERT(width > 0 && height > 0, "invalid wh");
  uiview_ = uiview_controller_.view;
  GHOST_ASSERT(uiview_, "uiview not valid");

  /* Initialize Metal device. */
  metal_view_.device = MTLCreateSystemDefaultDevice();

  /* Enable HDR/EDR Support. */
  CAMetalLayer *metalLayer = (CAMetalLayer *)metal_view_.layer;
  metalLayer.wantsExtendedDynamicRangeContent = YES;
  metalLayer.pixelFormat = MTLPixelFormatRGBA16Float;
  CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB);
  metalLayer.colorspace = colorspace;
  CGColorSpaceRelease(colorspace);

  setDrawingContextType(type);
  updateDrawingContext();
  activateDrawingContext();

  setTitle(title);

  /* Gesture recognizers. */
  [ghost_rootWindow registerGestureRecognizers];

  deferred_swap_buffers_count = 0;

  /* Deactive the parent (if it exists) and activate this one. */
  if (parent_window_) {
    parent_window_->requestToDeactivateWindow();
  }

  /* Make it the key window if there is no other window.
   * (Otherwise there will never be a call to drawInMTKView) */
  if (!system_ios_->current_active_window_) {
    request_to_make_active_ = true;
    makeKeyWindow();
  }
  /* Activate this window at the end of the next draw loop. */
  else {
    requestToActivateWindow();
  }
}

GHOST_WindowIOS::~GHOST_WindowIOS()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  releaseNativeHandles();

  /* Restore application control and display to parent window. */
  if (parent_window_) {
    parent_window_->requestToActivateWindow();
    parent_window_ = nil;
  }
  /* We have no choice but to resign, however this seems like it might cause issues. */
  if (system_ios_->current_active_window_ == this) {
    IOS_WINDOW_LOG(@"~GHOST_WindowIOS(): Warning, deactivating the active window %p?", this);
    requestToDeactivateWindow();
    resignKeyWindow();
  }

  if (metal_view_) {
    metal_view_.delegate = nil;
    [metal_view_ release];
    metal_view_ = nil;
  }
  if (uiview_) {
    [uiview_ release];
    uiview_ = nil;
  }

  /* Release window. */
  if (rootWindow) {
    [rootWindow release];
    rootWindow = nil;
  }
  if (uiview_controller_) {
    [uiview_controller_ release];
    uiview_controller_ = nil;
  }

  if (window_title_) {
    free(window_title_);
    window_title_ = nullptr;
  }

  [pool drain];
}

#pragma mark accessors

bool GHOST_WindowIOS::getValid() const
{
  MTKView *view = metal_view_;
  return GHOST_Window::getValid() && uiview_ != NULL && view != NULL;
}

void *GHOST_WindowIOS::getOSWindow() const
{
  return (void *)uiview_;
}

GHOST_TSuccess GHOST_WindowIOS::swapBuffers()
{
  deferred_swap_buffers_count++;
  return GHOST_kSuccess;
}

void GHOST_WindowIOS::flushDeferredSwapBuffers()
{
  if (deferred_swap_buffers_count) {

    /* These two messages should be made asserts when we've fixed all the issues. */
    if (!getValid()) {
      IOS_WINDOW_LOG(@"Ignoring swap (invalid) con(%p) (win=%p)", getContext(), this);
      return;
    }

    if (!is_active_window_) {
      IOS_WINDOW_LOG(@"Ignoring swap (not active window) con(%p) (win=%p)", getContext(), this);
      return;
    }

    IOS_WINDOW_LOG(@"Swapping (ui_View)%p (mtkView)%p con(%p) (win=%p) (sc=%d)",
                   uiview_,
                   metal_view_,
                   getContext(),
                   this,
                   deferred_swap_buffers_count);

    GHOST_ContextIOS *context = reinterpret_cast<GHOST_ContextIOS *>(getContext());
    context->swapBuffers();
    deferred_swap_buffers_count = 0;
  }
}

void GHOST_WindowIOS::beginFrame()
{
  GHOSTUIWindow *ui_window = (GHOSTUIWindow *)rootWindow;
  [ui_window beginFrame];
}

void GHOST_WindowIOS::endFrame()
{
  GHOSTUIWindow *ui_window = (GHOSTUIWindow *)rootWindow;
  [ui_window endFrame];
}

void GHOST_WindowIOS::setTitle(const char *title)
{
  if (window_title_) {
    free(window_title_);
    window_title_ = nullptr;
  }
  window_title_ = (char *)malloc(strlen(title) + 1);
  if (!window_title_) {
    GHOST_ASSERT(getValid(), "GHOST_WindowIOS::setTitle(): Failed to alloc mem for window title");
  }
  strcpy(window_title_, title);
  NSString *window_title = [NSString stringWithCString:title encoding:NSUTF8StringEncoding];
  uiview_controller_.title = window_title;
}

std::string GHOST_WindowIOS::getTitle() const
{
  return window_title_;
}

void GHOST_WindowIOS::needsDisplayUpdate()
{
  [uiview_ setNeedsDisplay];
}

void GHOST_WindowIOS::getWindowBounds(GHOST_Rect &bounds) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowIOS::getWindowBounds(): window invalid");

  CGRect screenRect = rootWindow.frame;
  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat screenWidth = screenRect.size.width * scale;
  CGFloat screenHeight = screenRect.size.height * scale;

  bounds.b_ = screenHeight;
  bounds.l_ = rootWindow.frame.origin.x;
  bounds.r_ = screenWidth;
  bounds.t_ = rootWindow.frame.origin.y;
}

void GHOST_WindowIOS::getClientBounds(GHOST_Rect &bounds) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowIOS::getWindowBounds(): window invalid");

  CGRect screenRect = rootWindow.frame;
  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat screenWidth = screenRect.size.width * scale;
  CGFloat screenHeight = screenRect.size.height * scale;

  bounds.b_ = screenHeight;
  bounds.l_ = 0;
  bounds.r_ = screenWidth;
  bounds.t_ = 0;
}

GHOST_TSuccess GHOST_WindowIOS::setClientWidth(uint32_t /*width*/)
{
  /* Ignore on iOS fow now. */
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowIOS::setClientHeight(uint32_t /*height*/)
{
  /* Ignore on iOS fow now. */
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowIOS::setClientSize(uint32_t /*width*/, uint32_t /*height*/)
{
  /* Ignore on iOS fow now. */
  return GHOST_kSuccess;
}

GHOST_TWindowState GHOST_WindowIOS::getState() const
{
  /* TODO: Implement. */
  return GHOST_kWindowStateNormal;
}

void GHOST_WindowIOS::screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const
{
  /* Pass through for fullscreen windows.
   * TODO: Support coordinate mapping for sized windows. */
  outX = inX;
  outY = inY;
}

void GHOST_WindowIOS::clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const
{
  /* Pass through for fullscreen windows.
   * TODO: Support coordinate mapping for sized windows. */
  outX = inX;
  outY = inY;
}

void GHOST_WindowIOS::screenToClientIntern(int32_t inX,
                                           int32_t inY,
                                           int32_t &outX,
                                           int32_t &outY) const
{
  /* Pass through for fullscreen windows.
   * TODO: Support coordinate mapping for sized windows. */
  outX = inX;
  outY = inY;
}

void GHOST_WindowIOS::clientToScreenIntern(int32_t inX,
                                           int32_t inY,
                                           int32_t &outX,
                                           int32_t &outY) const
{
  /* Pass through for fullscreen windows.
   * TODO: Support coordinate mapping for sized windows. */
  outX = inX;
  outY = inY;
}

/* called for event, when window leaves monitor to another */
void GHOST_WindowIOS::setNativePixelSize(void) {}

/**
 * \note Fullscreen switch is not actual fullscreen with display capture.
 * As this capture removes all OS X window manager features.
 *
 * Instead, the menu bar and the dock are hidden, and the window is made border-less and
 * enlarged. Thus, process switch, exposé, spaces, ... still work in fullscreen mode
 */
GHOST_TSuccess GHOST_WindowIOS::setState(GHOST_TWindowState /*state*/)
{
  // Ignore on iOS?
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowIOS::setModifiedState(bool isUnsavedChanges)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  [pool drain];
  return GHOST_Window::setModifiedState(isUnsavedChanges);
}

GHOST_TSuccess GHOST_WindowIOS::setOrder(GHOST_TWindowOrder /*order*/)
{
  /* TODO: Support or deprecate for iOS */
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  GHOST_ASSERT(getValid(), "GHOST_WindowIOS::setOrder(): window invalid");

  [pool drain];
  return GHOST_kSuccess;
}

#pragma mark Drawing context

GHOST_Context *GHOST_WindowIOS::newDrawingContext(GHOST_TDrawingContextType type)
{

  if (type == GHOST_kDrawingContextTypeMetal) {

    GHOST_Context *context = new GHOST_ContextIOS(want_context_params_, uiview_, metal_view_);

    if (context->initializeDrawingContext())
      return context;
    else
      delete context;
  }

  return NULL;
}

#pragma mark invalidate

GHOST_TSuccess GHOST_WindowIOS::invalidate()
{
  GHOST_ASSERT(getValid(), "GHOST_WindowIOS::invalidate(): window invalid");
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [pool drain];
  return GHOST_kSuccess;
}

#pragma mark Progress bar

GHOST_TSuccess GHOST_WindowIOS::setProgressBar(float /*progress*/)
{
  return GHOST_kSuccess;
}

static void postNotification() {}

GHOST_TSuccess GHOST_WindowIOS::endProgressBar()
{
  return GHOST_kSuccess;
}

#pragma mark Cursor handling

void GHOST_WindowIOS::loadCursor(bool /*visible*/, GHOST_TStandardCursor /*shape*/) const {}

bool GHOST_WindowIOS::isDialog() const
{
  return is_dialog_;
}

GHOST_TSuccess GHOST_WindowIOS::setWindowCursorVisibility(bool /*visible*/)
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowIOS::setWindowCursorGrab(GHOST_TGrabCursorMode /*mode*/)
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowIOS::setWindowCursorShape(GHOST_TStandardCursor /*shape*/)
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowIOS::hasCursorShape(GHOST_TStandardCursor /*shape*/)
{
  return GHOST_kSuccess;
}

/** Reverse the bits in a uint16_t */
static uint16_t uns16ReverseBits(uint16_t shrt)
{
  shrt = ((shrt >> 1) & 0x5555) | ((shrt << 1) & 0xAAAA);
  shrt = ((shrt >> 2) & 0x3333) | ((shrt << 2) & 0xCCCC);
  shrt = ((shrt >> 4) & 0x0F0F) | ((shrt << 4) & 0xF0F0);
  shrt = ((shrt >> 8) & 0x00FF) | ((shrt << 8) & 0xFF00);
  return shrt;
}

GHOST_TSuccess GHOST_WindowIOS::setWindowCustomCursorShape(const uint8_t * /*bitmap*/,
                                                           const uint8_t * /*mask*/,
                                                           const int /*size*/[2],
                                                           const int /*hot_spot*/[2],
                                                           bool /*canInvertColor*/)
{
  /* Passthrough for iOS. */
  return GHOST_kSuccess;
}

uint16_t GHOST_WindowIOS::getDPIHint()
{
  /* IOS_FIXME: 96 is the default but the UI is too small to see
   * easily - especially on smaller iPads.
   * 192 looks good but cannot fit all the menus in so this value
   * is the compromise. It's possible we should look at the size of
   * screen and adjust this value dynamically. 192 might be OK for
   * the larger iPads but for now 144 seems to work OK. */
  return 144;
}

GHOST_TSuccess GHOST_WindowIOS::popupOnscreenKeyboard(
    const GHOST_KeyboardProperties &keyboard_properties)
{
  GHOSTUIWindow *ghost_rootWindow = (GHOSTUIWindow *)rootWindow;
  return [ghost_rootWindow popupOnscreenKeyboard:keyboard_properties];
}

GHOST_TSuccess GHOST_WindowIOS::hideOnscreenKeyboard()
{
  GHOSTUIWindow *ghost_rootWindow = (GHOSTUIWindow *)rootWindow;
  return [ghost_rootWindow hideOnscreenKeyboard];
}

const char *GHOST_WindowIOS::getLastKeyboardString()
{
  GHOSTUIWindow *ghost_rootWindow = (GHOSTUIWindow *)rootWindow;
  return [ghost_rootWindow getLastKeyboardString];
}

UITextField *GHOST_WindowIOS::getUITextField()
{
  GHOSTUIWindow *ghost_rootWindow = (GHOSTUIWindow *)rootWindow;
  return [ghost_rootWindow getUITextField];
}

const GHOST_TabletData GHOST_WindowIOS::getTabletData()
{
  GHOSTUIWindow *ghost_rootWindow = (GHOSTUIWindow *)rootWindow;
  return [ghost_rootWindow getTabletData];
}

/* This is the size of the window pre-scaled */
CGSize GHOST_WindowIOS::getLogicalWindowSize()
{
  return metal_view_.frame.size;
}

/* This is the size of the window post-scaled */
CGSize GHOST_WindowIOS::getNativeWindowSize()
{
  return metal_view_.drawableSize;
}

float GHOST_WindowIOS::getWindowScaleFactor()
{
  return [[UIScreen mainScreen] scale];
}

/* Indicate that we want this window to be the next active one. */
void GHOST_WindowIOS::requestToActivateWindow()
{
  /* Check we're not already active. */
  if (system_ios_->current_active_window_ != this) {
    /* Replace any outstanding requests. */
    if (system_ios_->next_active_window_) {
      system_ios_->next_active_window_->requestToDeactivateWindow();
    }
    request_to_make_active_ = true;
    system_ios_->next_active_window_ = this;
  }
}

void GHOST_WindowIOS::requestToDeactivateWindow()
{
  if (system_ios_->next_active_window_ == this) {
    IOS_WINDOW_LOG(@"requestToDeactivateWindow(): Has something gone wrong? %p", this);
    system_ios_->next_active_window_ = nullptr;
  }
  request_to_make_active_ = false;
}

bool GHOST_WindowIOS::makeKeyWindow()
{
  if (!getValid()) {
    IOS_WINDOW_LOG(@"Failed to activate (invalid) con(%p) (win=%p)", getContext(), this);
    return false;
  }

  GHOST_ContextIOS *context = reinterpret_cast<GHOST_ContextIOS *>(getContext());
  GHOST_ASSERT(rootWindow != nil, "GHOST_WindowIOS::makeKeyWindow() root window required");
  GHOST_ASSERT(context != nullptr, "GHOST_WindowIOS::makeKeyWindow() context required");
  GHOST_ASSERT(request_to_make_active_,
               "GHOST_WindowIOS::makeKeyWindow() must request activation first");

  /* Make window primary visible window. */
  [rootWindow makeKeyAndVisible];
  /* Enable the drawInMTKView() calls for this window. */
  metal_view_.paused = NO;

  IOS_WINDOW_LOG(@"Key Window: (ui_View)%p (mtkView)%p con(%p) (win=%p)",
                 uiview_,
                 metal_view_,
                 getContext(),
                 this);

  system_ios_->current_active_window_ = this;
  is_active_window_ = true;
  request_to_make_active_ = false;
  return true;
}

void GHOST_WindowIOS::resignKeyWindow()
{
  GHOST_ASSERT(system_ios_->current_active_window_ == this,
               "GHOST_WindowIOS::resignKeyWindow(): Can only resign current active window");
  GHOST_ASSERT(is_active_window_,
               "GHOST_WindowIOS::resignKeyWindow(): Can't resign non active window");
  GHOST_ASSERT(!request_to_make_active_,
               "GHOST_WindowIOS::resignKeyWindow(): activation request outstanding");

  /* Disable the drawInMTKView() calls for this window. */
  metal_view_.paused = YES;
  /* Wait until any outstanding presents in flight are done. */
  while (uiview_controller_.beingPresented) {
  }
  IOS_WINDOW_LOG(@"Resigning Key Window: (ui_View)%p (mtkView)%p con(%p) (win=%p)",
                 uiview_,
                 metal_view_,
                 getContext(),
                 this);
  is_active_window_ = false;
  system_ios_->current_active_window_ = nullptr;
}

CGPoint GHOST_WindowIOS::scalePointToWindow(CGPoint &point)
{
  CGPoint scaled_point;
  scaled_point.x = point.x * getWindowScaleFactor();
  scaled_point.y = point.y * getWindowScaleFactor();
  return scaled_point;
}

#ifdef WITH_INPUT_IME
void GHOST_WindowIOS::beginIME(
    int32_t /*x*/, int32_t /*y*/, int32_t /*w*/, int32_t /*h*/, bool /*completed*/)
{
  /* Passthrough for iOS. */
}

void GHOST_WindowIOS::endIME()
{
  /* Passthrough for iOS. */
}
#endif /* WITH_INPUT_IME */
