/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
*/

/** \file ghost/intern/GHOST_ImeWin32.h
 *  \ingroup GHOST
 */

#ifndef __GHOST_IME_H__
#define __GHOST_IME_H__

#include <windows.h>

#include <string>

#include "GHOST_Event.h"
#include "GHOST_Rect.h"
#include <vector>

class GHOST_EventIME : public GHOST_Event
{
public:
	/**
	 * Constructor.
	 * \param msec	The time this event was generated.
	 * \param type	The type of key event.
	 * \param key	The key code of the key.
	 */
	GHOST_EventIME(GHOST_TUns64 msec,
	               GHOST_TEventType type,
	               GHOST_IWindow *window, void *customdata)
	               : GHOST_Event(msec, type, window)
	{
		this->m_data = customdata;
	}

};


/**
 * This header file defines a struct and a class used for encapsulating IMM32
 * APIs, controls IMEs attached to a window, and enables the 'on-the-spot'
 * input without deep knowledge about the APIs, i.e. knowledge about the
 * language-specific and IME-specific behaviors.
 * The following items enumerates the simplest steps for an (window)
 * application to control its IMEs with the struct and the class defined
 * this file.
 * 1. Add an instance of the GHOST_ImeWin32 class to its window class.
 *    (The GHOST_ImeWin32 class needs a window handle.)
 * 2. Add messages handlers listed in the following subsections, follow the
 *    instructions written in each subsection, and use the GHOST_ImeWin32 class.
 * 2.1. WM_IME_SETCONTEXT (0x0281)
 *      Call the functions listed below:
 *      - GHOST_ImeWin32::CreateImeWindow();
 *      - GHOST_ImeWin32::CleanupComposition(), and;
 *      - GHOST_ImeWin32::SetImeWindowStyle().
 *      An application MUST prevent from calling ::DefWindowProc().
 * 2.2. WM_IME_STARTCOMPOSITION (0x010D)
 *      Call the functions listed below:
 *      - GHOST_ImeWin32::CreateImeWindow(), and;
 *      - GHOST_ImeWin32::ResetComposition().
 *      An application MUST prevent from calling ::DefWindowProc().
 * 2.3. WM_IME_COMPOSITION (0x010F)
 *      Call the functions listed below:
 *      - GHOST_ImeWin32::UpdateImeWindow();
 *      - GHOST_ImeWin32::GetResult();
 *      - GHOST_ImeWin32::GetComposition(), and;
 *      - GHOST_ImeWin32::ResetComposition() (optional).
 *      An application MUST prevent from calling ::DefWindowProc().
 * 2.4. WM_IME_ENDCOMPOSITION (0x010E)
 *      Call the functions listed below:
 *      - GHOST_ImeWin32::ResetComposition(), and;
 *      - GHOST_ImeWin32::DestroyImeWindow().
 *      An application CAN call ::DefWindowProc().
 * 2.5. WM_INPUTLANGCHANGE (0x0051)
 *      Call the functions listed below:
 *      - GHOST_ImeWin32::SetInputLanguage().
 *      An application CAN call ::DefWindowProc().
 */

/* This struct represents the status of an ongoing composition. */
struct ImeComposition {
	/* Represents the cursor position in the IME composition. */
	int cursor_position;

	/* Represents the position of the beginning of the selection */
	int target_start;

	/* Represents the position of the end of the selection */
	int target_end;

	/**
	 * Represents the type of the string in the 'ime_string' parameter.
	 * Its possible values and description are listed below:
	 *   Value         Description
	 *   0             The parameter is not used.
	 *   GCS_RESULTSTR The parameter represents a result string.
	 *   GCS_COMPSTR   The parameter represents a composition string.
	 */
	int string_type;

	/* Represents the string retrieved from IME (Input Method Editor) */
	std::wstring ime_string;
	std::vector<char> utf8_buf;
	std::vector<unsigned char> format;
};

/**
 * This class controls the IMM (Input Method Manager) through IMM32 APIs and
 * enables it to retrieve the string being controled by the IMM. (I wrote
 * a note to describe the reason why I do not use 'IME' but 'IMM' below.)
 * NOTE(hbono):
 *   Fortunately or unfortunately, TSF (Text Service Framework) and
 *   CUAS (Cicero Unaware Application Support) allows IMM32 APIs for
 *   retrieving not only the inputs from IMEs (Input Method Editors), used
 *   only for inputting East-Asian language texts, but also the ones from
 *   tablets (on Windows XP Tablet PC Edition and Windows Vista), voice
 *   recognizers (e.g. ViaVoice and Microsoft Office), etc.
 *   We can disable TSF and CUAS in Windows XP Tablet PC Edition. On the other
 *   hand, we can NEVER disable either TSF or CUAS in Windows Vista, i.e.
 *   THIS CLASS IS NOT ONLY USED ON THE INPUT CONTEXTS OF EAST-ASIAN
 *   LANGUAGES BUT ALSO USED ON THE INPUT CONTEXTS OF ALL LANGUAGES.
 */
class GHOST_ImeWin32 {
public:
	GHOST_ImeWin32();
	~GHOST_ImeWin32();

	/* Retrieves whether or not there is an ongoing composition. */
	bool is_composing() const {return is_composing_;}

	/**
	 * Retrieves the input language from Windows and update it.
	 * Return values
	 *   * true
	 *     The given input language has IMEs.
	 *   * false
	 *     The given input language does not have IMEs.
	 */
	bool SetInputLanguage();

	/**
	 * Create the IME windows, and allocate required resources for them.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 */
	void CreateImeWindow(HWND window_handle);

	/**
	 * Update the style of the IME windows.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 *   * message [in] (UINT)
	 *   * wparam [in] (WPARAM)
	 *   * lparam [in] (LPARAM)
	 *     Represent the windows message of the caller.
	 *     These parameters are used for verifying if this function is called
	 *     in a handler function for WM_IME_SETCONTEXT messages because this
	 *     function uses ::DefWindowProc() to update the style.
	 *     A caller just has to pass the input parameters for the handler
	 *     function without modifications.
	 *   * handled [out] (BOOL*)
	 *     Returns ::DefWindowProc() is really called in this function.
	 *     PLEASE DO NOT CALL ::DefWindowProc() IF THIS VALUE IS TRUE!
	 *     All the window styles set in this function are over-written when
	 *     calling ::DefWindowProc() after returning this function.
	 */
	void SetImeWindowStyle(HWND window_handle, UINT message,
	                       WPARAM wparam, LPARAM lparam, BOOL* handled);

	/**
	 * Destroy the IME windows and all the resources attached to them.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 */
	void DestroyImeWindow(HWND window_handle);

	/**
	 * Update the position of the IME windows.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 */
	void UpdateImeWindow(HWND window_handle);

	/**
	 * Clean up the all resources attached to the given GHOST_ImeWin32 object, and
	 * reset its composition status.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 */
	void CleanupComposition(HWND window_handle);

	/**
	 * Reset the composition status.
	 * Cancel the ongoing composition if it exists.
	 * NOTE(hbono): This method does not release the allocated resources.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 */
	void ResetComposition(HWND window_handle);

	/**
	 * Retrieve a composition result of the ongoing composition if it exists.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 *   * lparam [in] (LPARAM)
	 *     Specifies the updated members of the ongoing composition, and must be
	 *     the same parameter of a WM_IME_COMPOSITION message handler.
	 *     This parameter is used for checking if the ongoing composition has
	 *     its result string,
	 *   * composition [out] (ImeComposition)
	 *     Represents the struct contains the composition result.
	 * Return values
	 *   * true
	 *     The ongoing composition has a composition result.
	 *   * false
	 *     The ongoing composition does not have composition results.
	 * Remarks
	 *   This function is designed for being called from WM_IME_COMPOSITION
	 *   message handlers.
	 */
	bool GetResult(HWND window_handle, LPARAM lparam,
	               ImeComposition* composition);

	/**
	 * Retrieve the current composition status of the ongoing composition.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 *   * lparam [in] (LPARAM)
	 *     Specifies the updated members of the ongoing composition, and must be
	 *     the same parameter of a WM_IME_COMPOSITION message handler.
	 *     This parameter is used for checking if the ongoing composition has
	 *     its result string,
	 *   * composition [out] (ImeComposition)
	 *     Represents the struct contains the composition status.
	 * Return values
	 *   * true
	 *     The status of the ongoing composition is updated.
	 *   * false
	 *     The status of the ongoing composition is not updated.
	 * Remarks
	 *   This function is designed for being called from WM_IME_COMPOSITION
	 *   message handlers.
	 */
	bool GetComposition(HWND window_handle, LPARAM lparam,
	                    ImeComposition* composition);

	/**
	 * Enable the IME attached to the given window, i.e. allows user-input
	 * events to be dispatched to the IME.
	 * In Chrome, this function is used when:
	 *   * a renderer process moves its input focus to another edit control, or;
	 *   * a renrerer process moves the position of the focused edit control.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 *   * caret_rect [in] (const gfx::Rect&)
	 *     Represent the rectangle of the input caret.
	 *     This rectangle is used for controlling the positions of IME windows.
	 *   * complete [in] (bool)
	 *     Represents whether or not to complete the ongoing composition.
	 *     + true
	 *       After finishing the ongoing composition and close its IME windows,
	 *       start another composition and display its IME windows to the given
	 *       position.
	 *     + false
	 *       Just move the IME windows of the ongoing composition to the given
	 *       position without finishing it.
	 */
	void BeginIME(HWND window_handle,
	               const GHOST_Rect& caret_rect,
	               bool complete);

	/**
	 * Disable the IME attached to the given window, i.e. prohibits any user-input
	 * events from being dispatched to the IME.
	 * In Chrome, this function is used when:
	 *   * a renreder process sets its input focus to a password input.
	 * Parameters
	 *   * window_handle [in] (HWND)
	 *     Represents the window handle of the caller.
	 */
	void EndIME(HWND window_handle);

	/* Updatg resultInfo and compInfo */
	void UpdateInfo(HWND window_handle);

	/* disable ime when start up */
	void CheckFirst(HWND window_handle);

	ImeComposition resultInfo, compInfo;
	GHOST_TEventImeData eventImeData;

protected:
	/* Determines whether or not the given attribute represents a target (a.k.a. a selection). */
	bool IsTargetAttribute(char attribute) const {
		return (attribute == ATTR_TARGET_CONVERTED ||
		        attribute == ATTR_TARGET_NOTCONVERTED);
	}

	/* Retrieve the target area. */
	void GetCaret(HIMC imm_context, LPARAM lparam,
	              ImeComposition* composition);

	/* Update the position of the IME windows. */
	void MoveImeWindow(HWND window_handle, HIMC imm_context);

	/* Complete the ongoing composition if it exists. */
	void CompleteComposition(HWND window_handle, HIMC imm_context);

	/* Retrieve a string from the IMM. */
	bool GetString(HIMC imm_context, WPARAM lparam, int type,
	               ImeComposition* composition);

private:
	/**
	 * Represents whether or not there is an ongoing composition in a browser
	 * process, i.e. whether or not a browser process is composing a text.
	 */
	bool is_composing_;

	/**
	 * This value represents whether or not the current input context has IMEs.
	 * The following table shows the list of IME status:
	 *   Value  Description
	 *   false  The current input language does not have IMEs.
	 *   true   The current input language has IMEs.
	 */
	bool ime_status_;

	/**
	 * The current input Language ID retrieved from Windows, which consists of:
	 *   * Primary Language ID (bit 0 to bit 9), which shows a natunal language
	 *     (English, Korean, Chinese, Japanese, etc.) and;
	 *   * Sub-Language ID (bit 10 to bit 15), which shows a geometrical region
	 *     the language is spoken (For English, United States, United Kingdom,
	 *     Australia, Canada, etc.)
	 * The following list enumerates some examples for the Language ID:
	 *   * "en-US" (0x0409)
	 *     MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
	 *   * "ko-KR" (0x0412)
	 *     MAKELANGID(LANG_KOREAN,  SUBLANG_KOREAN);
	 *   * "zh-TW" (0x0404)
	 *     MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
	 *   * "zh-CN" (0x0804)
	 *     MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
	 *   * "ja-JP" (0x0411)
	 *     MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN), etc.
	 *   (See <winnt.h> for other available values.)
	 * This Language ID is used for processing language-specific operations in
	 * IME functions.
	 */
	LANGID input_language_id_;

	/**
	 * Represents whether or not the current input context has created a system
	 * caret to set the position of its IME candidate window.
	 *   * true: it creates a system caret.
	 *   * false: it does not create a system caret.
	 */
	bool system_caret_;

	/* The rectangle of the input caret retrieved from a renderer process. */
	GHOST_Rect caret_rect_;

	/* used for disable ime when start up */
	bool is_first, is_enable;
};

#endif   * __GHOST_IME_H__
