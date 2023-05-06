/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/* The Carbon API is still needed to check if the Input Source (Input Method or IME) is valid. */
#ifdef WITH_INPUT_IME
#  import <Carbon/Carbon.h>
#endif

/* NSView subclass for drawing and handling input.
 *
 * COCOA_VIEW_BASE_CLASS will be either NSView or NSOpenGLView depending if
 * we use a Metal or OpenGL layer for drawing in the view. We use macros
 * to defined classes for each case, so we don't have to duplicate code as
 * Objective-C does not have multiple inheritance. */

// We need to subclass it in order to give Cocoa the feeling key events are trapped
@interface COCOA_VIEW_CLASS : COCOA_VIEW_BASE_CLASS <NSTextInputClient>
{
  GHOST_SystemCocoa *systemCocoa;
  GHOST_WindowCocoa *associatedWindow;

  bool composing;
  NSString *composing_text;

#ifdef WITH_INPUT_IME
  struct {
    GHOST_ImeStateFlagCocoa state_flag;
    NSRect candidate_window_position;

    /* Event data. */
    GHOST_TEventImeData event;
    std::string result;
    std::string composite;
    std::string combined_result;
  } ime;
#endif
}
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa
                    windowCocoa:(GHOST_WindowCocoa *)winCocoa;

#ifdef WITH_INPUT_IME
- (void)beginIME:(int32_t)x y:(int32_t)y w:(int32_t)w h:(int32_t)h completed:(bool)completed;

- (void)endIME;
#endif
@end

@implementation COCOA_VIEW_CLASS

- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa
                    windowCocoa:(GHOST_WindowCocoa *)winCocoa
{
  systemCocoa = sysCocoa;
  associatedWindow = winCocoa;

  composing = false;
  composing_text = nil;

#ifdef WITH_INPUT_IME
  ime.state_flag = 0;
  ime.candidate_window_position = NSZeroRect;
  ime.event.cursor_position = -1;
  ime.event.target_start = -1;
  ime.event.target_end = -1;

  /* Register a function to be executed when Input Method is changed using
   * 'Control + Space' or language-specific keys (such as 'Eisu / Kana' key for Japanese). */
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(ImeDidChangeCallback:)
                 name:NSTextInputContextKeyboardSelectionDidChangeNotification
               object:nil];
#endif
}

- (BOOL)acceptsFirstResponder
{
  return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
  return YES;
}

// The trick to prevent Cocoa from complaining (beeping)
- (void)keyDown:(NSEvent *)event
{
#ifdef WITH_INPUT_IME
  [self checkKeyCodeIsControlChar:event];
  const bool ime_process = [self isProcessedByIme];
#else
  const bool ime_process = false;
#endif

  if (!ime_process) {
    systemCocoa->handleKeyEvent(event);
  }

  /* Start or continue composing? */
  if ([[event characters] length] == 0 || [[event charactersIgnoringModifiers] length] == 0 ||
      composing || ime_process)
  {
    composing = YES;

    // interpret event to call insertText
    [self interpretKeyEvents:[NSArray arrayWithObject:event]];  // calls insertText

#ifdef WITH_INPUT_IME
    // For Korean input, control characters are also processed by handleKeyEvent.
    const int controlCharForKorean = (GHOST_IME_COMPOSITION_EVENT | GHOST_IME_RESULT_EVENT |
                                      GHOST_IME_KEY_CONTROL_CHAR);
    if (((ime.state_flag & controlCharForKorean) == controlCharForKorean)) {
      systemCocoa->handleKeyEvent(event);
    }

    ime.state_flag &= ~(GHOST_IME_COMPOSITION_EVENT | GHOST_IME_RESULT_EVENT);

    ime.combined_result.clear();
#endif

    return;
  }
}

- (void)keyUp:(NSEvent *)event
{
  systemCocoa->handleKeyEvent(event);
}

- (void)flagsChanged:(NSEvent *)event
{
  systemCocoa->handleKeyEvent(event);
}

- (void)mouseDown:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)mouseUp:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)rightMouseDown:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)rightMouseUp:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)mouseMoved:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)mouseDragged:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)rightMouseDragged:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)scrollWheel:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)otherMouseDown:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)otherMouseUp:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)otherMouseDragged:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)magnifyWithEvent:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)smartMagnifyWithEvent:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)rotateWithEvent:(NSEvent *)event
{
  systemCocoa->handleMouseEvent(event);
}

- (void)tabletPoint:(NSEvent *)event
{
  systemCocoa->handleTabletEvent(event, [event type]);
}

- (void)tabletProximity:(NSEvent *)event
{
  systemCocoa->handleTabletEvent(event, [event type]);
}

- (BOOL)isOpaque
{
  return YES;
}

- (void)drawRect:(NSRect)rect
{
  if ([self inLiveResize]) {
    /* Don't redraw while in live resize */
  }
  else {
    [super drawRect:rect];
    systemCocoa->handleWindowEvent(GHOST_kEventWindowUpdate, associatedWindow);

    /* For some cases like entering full-screen we need to redraw immediately
     * so our window does not show blank during the animation */
    if (associatedWindow->getImmediateDraw())
      systemCocoa->dispatchEvents();
  }
}

// Text input

- (void)composing_free
{
  composing = NO;

  if (composing_text) {
    [composing_text release];
    composing_text = nil;
  }
}

// Processes the Result String sent from the Input Method.
- (void)insertText:(id)chars replacementRange:(NSRange)replacementRange
{
  [self composing_free];

#ifdef WITH_INPUT_IME
  if (ime.state_flag & GHOST_IME_ENABLED) {
    if (!(ime.state_flag & GHOST_IME_COMPOSING)) {
      [self processImeEvent:GHOST_kEventImeCompositionStart];
    }

    // For Chinese and Korean input, insertText may be executed twice with a single keyDown.
    if (ime.state_flag & GHOST_IME_RESULT_EVENT) {
      ime.combined_result += [self convertNSString:chars];
    }
    else {
      ime.combined_result = [self convertNSString:chars];
    }

    [self setImeResult:ime.combined_result];

    /* For Korean input, both "Result Event" and "Composition Event"
     * can occur in a single keyDown. */
    if (![self ime_did_composition]) {
      [self processImeEvent:GHOST_kEventImeComposition];
    }
    ime.state_flag |= GHOST_IME_RESULT_EVENT;

    [self processImeEvent:GHOST_kEventImeCompositionEnd];
    ime.state_flag &= ~GHOST_IME_COMPOSING;
  }
#endif
}

// Processes the Composition String sent from the Input Method.
- (void)setMarkedText:(id)chars
        selectedRange:(NSRange)range
     replacementRange:(NSRange)replacementRange
{
  [self composing_free];

  if ([chars length] == 0) {
#ifdef WITH_INPUT_IME
    // Processes when the last Composition String is deleted.
    if (ime.state_flag & GHOST_IME_COMPOSING) {
      [self setImeResult:std::string()];
      [self processImeEvent:GHOST_kEventImeComposition];
      [self processImeEvent:GHOST_kEventImeCompositionEnd];
      ime.state_flag &= ~GHOST_IME_COMPOSING;
    }
#endif

    return;
  }

  // start composing
  composing = YES;
  composing_text = [chars copy];

  // chars of markedText by Input Method is an instance of NSAttributedString
  if ([chars isKindOfClass:[NSAttributedString class]]) {
    composing_text = [[chars string] copy];
  }

  // if empty, cancel
  if ([composing_text length] == 0)
    [self composing_free];

#ifdef WITH_INPUT_IME
  if (ime.state_flag & GHOST_IME_ENABLED) {
    if (!(ime.state_flag & GHOST_IME_COMPOSING)) {
      ime.state_flag |= GHOST_IME_COMPOSING;
      [self processImeEvent:GHOST_kEventImeCompositionStart];
    }

    [self setImeComposition:composing_text selectedRange:range];

    // For Korean input, setMarkedText may be executed twice with a single keyDown.
    if (![self ime_did_composition]) {
      ime.state_flag |= GHOST_IME_COMPOSITION_EVENT;
      [self processImeEvent:GHOST_kEventImeComposition];
    }
  }
#endif
}

- (void)unmarkText
{
  [self composing_free];
}

- (BOOL)hasMarkedText
{
  return (composing) ? YES : NO;
}

- (void)doCommandBySelector:(SEL)selector
{
}

- (BOOL)isComposing
{
  return composing;
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actualRange
{
  return [[[NSAttributedString alloc] init] autorelease];
}

- (NSRange)markedRange
{
  unsigned int length = (composing_text) ? [composing_text length] : 0;

  if (composing)
    return NSMakeRange(0, length);

  return NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
  unsigned int length = (composing_text) ? [composing_text length] : 0;
  return NSMakeRange(0, length);
}

// Specify the position where the Chinese and Japanese candidate windows are displayed.
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
#ifdef WITH_INPUT_IME
  if (ime.state_flag & GHOST_IME_ENABLED) {
    return ime.candidate_window_position;
  }
#endif
  return NSZeroRect;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
  return NSNotFound;
}

- (NSArray *)validAttributesForMarkedText
{
  return [NSArray array];
}

#ifdef WITH_INPUT_IME
- (void)checkImeEnabled
{
  ime.state_flag &= ~GHOST_IME_ENABLED;

  if (ime.state_flag & GHOST_IME_INPUT_FOCUSED) {
    /* Since there are no functions in Cocoa API,
     * we will use the functions in the Carbon API. */
    TISInputSourceRef currentKeyboardInputSource = TISCopyCurrentKeyboardInputSource();
    bool ime_enabled = !CFBooleanGetValue((CFBooleanRef)TISGetInputSourceProperty(
        currentKeyboardInputSource, kTISPropertyInputSourceIsASCIICapable));
    CFRelease(currentKeyboardInputSource);

    if (ime_enabled) {
      ime.state_flag |= GHOST_IME_ENABLED;
      return;
    }
  }
  return;
}

- (void)ImeDidChangeCallback:(NSNotification *)notification
{
  [self checkImeEnabled];
}

- (void)setImeCandidateWinPos:(int32_t)x y:(int32_t)y w:(int32_t)w h:(int32_t)h
{
  int32_t outX, outY;
  associatedWindow->clientToScreen(x, y, outX, outY);
  ime.candidate_window_position = NSMakeRect((CGFloat)outX, (CGFloat)outY, (CGFloat)w, (CGFloat)h);
}

- (void)beginIME:(int32_t)x y:(int32_t)y w:(int32_t)w h:(int32_t)h completed:(bool)completed
{
  ime.state_flag |= GHOST_IME_INPUT_FOCUSED;
  [self checkImeEnabled];
  [self setImeCandidateWinPos:x y:y w:w h:h];
}

- (void)endIME
{
  ime.state_flag = 0;
  ime.result.clear();
  ime.composite.clear();

  [self unmarkText];
  [[NSTextInputContext currentInputContext] discardMarkedText];
}

- (void)processImeEvent:(GHOST_TEventType)imeEventType
{
  ime.event.result_len = (GHOST_TUserDataPtr)ime.result.size();
  ime.event.result = (GHOST_TUserDataPtr)ime.result.c_str();
  ime.event.composite_len = (GHOST_TUserDataPtr)ime.composite.size();
  ime.event.composite = (GHOST_TUserDataPtr)ime.composite.c_str();

  GHOST_Event *event = new GHOST_EventIME(
      systemCocoa->getMilliSeconds(), imeEventType, associatedWindow, &ime.event);
  systemCocoa->pushEvent(event);
}

- (std::string)convertNSString:(NSString *)inString
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  std::string str([inString UTF8String]);
  [pool drain];
  return str;
}

- (void)setImeComposition:(NSString *)inString selectedRange:(NSRange)range
{
  ime.composite = [self convertNSString:inString];

  // For Korean input, both "Result Event" and "Composition Event" can occur in a single keyDown.
  if (!(ime.state_flag & GHOST_IME_RESULT_EVENT)) {
    ime.result.clear();
  }

  /* The target string is equivalent to the string in selectedRange of setMarkedText.
   * The cursor is displayed at the beginning of the target string. */
  char *front_string = (char *)[[inString substringWithRange:NSMakeRange(0, range.location)]
      UTF8String];
  char *selected_string = (char *)[[inString substringWithRange:range] UTF8String];
  ime.event.cursor_position = strlen(front_string);
  ime.event.target_start = ime.event.cursor_position;
  ime.event.target_end = ime.event.target_start + strlen(selected_string);
}

- (void)setImeResult:(std::string)result
{
  ime.result = result;
  ime.composite.clear();
  ime.event.cursor_position = -1;
  ime.event.target_start = -1;
  ime.event.target_end = -1;
}

- (void)checkKeyCodeIsControlChar:(NSEvent *)event
{
  ime.state_flag &= ~GHOST_IME_KEY_CONTROL_CHAR;

  /* Don't use IME for command and ctrl key combinations, these are shortcuts. */
  if ([event modifierFlags] & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) {
    ime.state_flag |= GHOST_IME_KEY_CONTROL_CHAR;
    return;
  }

  /* Don't use IME for these control keys. */
  switch ([event keyCode]) {
    case kVK_ANSI_KeypadEnter:
    case kVK_ANSI_KeypadClear:
    case kVK_F1:
    case kVK_F2:
    case kVK_F3:
    case kVK_F4:
    case kVK_F5:
    case kVK_F6:
    case kVK_F7:
    case kVK_F8:
    case kVK_F9:
    case kVK_F10:
    case kVK_F11:
    case kVK_F12:
    case kVK_F13:
    case kVK_F14:
    case kVK_F15:
    case kVK_F16:
    case kVK_F17:
    case kVK_F18:
    case kVK_F19:
    case kVK_F20:
    case kVK_UpArrow:
    case kVK_DownArrow:
    case kVK_LeftArrow:
    case kVK_RightArrow:
    case kVK_Return:
    case kVK_Delete:
    case kVK_ForwardDelete:
    case kVK_Escape:
    case kVK_Tab:
    case kVK_Home:
    case kVK_End:
    case kVK_PageUp:
    case kVK_PageDown:
    case kVK_VolumeUp:
    case kVK_VolumeDown:
    case kVK_Mute:
      ime.state_flag |= GHOST_IME_KEY_CONTROL_CHAR;
      return;
  }
}

- (bool)ime_did_composition
{
  return (ime.state_flag & GHOST_IME_COMPOSITION_EVENT) ||
         (ime.state_flag & GHOST_IME_RESULT_EVENT);
}

/* Even if IME is enabled, when not composing, control characters
 * (such as arrow, enter, delete) are handled by handleKeyEvent. */
- (bool)isProcessedByIme
{
  return (
      (ime.state_flag & GHOST_IME_ENABLED) &&
      ((ime.state_flag & GHOST_IME_COMPOSING) || !(ime.state_flag & GHOST_IME_KEY_CONTROL_CHAR)));
}
#endif /* WITH_INPUT_IME */

@end
