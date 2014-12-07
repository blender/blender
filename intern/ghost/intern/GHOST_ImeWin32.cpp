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


/** \file ghost/intern/GHOST_ImeWin32.cpp
 *  \ingroup GHOST
 */


#include "GHOST_C-api.h"
#include "GHOST_ImeWin32.h"
#include "GHOST_WindowWin32.h"
#include "utfconv.h"


GHOST_ImeWin32::GHOST_ImeWin32()
    : ime_status_(false),
      input_language_id_(LANG_USER_DEFAULT),
      is_composing_(false),
      system_caret_(false),
      caret_rect_(-1, -1, 0, 0),
      is_first(true),
      is_enable(true)
{
}


GHOST_ImeWin32::~GHOST_ImeWin32()
{
}


bool GHOST_ImeWin32::SetInputLanguage()
{
	/**
	 * Retrieve the current keyboard layout from Windows and determine whether
	 * or not the current input context has IMEs.
	 * Also save its input language for language-specific operations required
	 * while composing a text.
	 */
	HKL keyboard_layout = ::GetKeyboardLayout(0);
	input_language_id_ = reinterpret_cast<LANGID>(keyboard_layout);
	ime_status_ = ::ImmIsIME(keyboard_layout);
	return ime_status_;
}


void GHOST_ImeWin32::CreateImeWindow(HWND window_handle)
{
	/**
	 * When a user disables TSF (Text Service Framework) and CUAS (Cicero
	 * Unaware Application Support), Chinese IMEs somehow ignore function calls
	 * to ::ImmSetCandidateWindow(), i.e. they do not move their candidate
	 * window to the position given as its parameters, and use the position
	 * of the current system caret instead, i.e. it uses ::GetCaretPos() to
	 * retrieve the position of their IME candidate window.
	 * Therefore, we create a temporary system caret for Chinese IMEs and use
	 * it during this input context.
	 * Since some third-party Japanese IME also uses ::GetCaretPos() to determine
	 * their window position, we also create a caret for Japanese IMEs.
	 */
	if (PRIMARYLANGID(input_language_id_) == LANG_CHINESE ||
		PRIMARYLANGID(input_language_id_) == LANG_JAPANESE) {
		if (!system_caret_) {
			if (::CreateCaret(window_handle, NULL, 1, 1)) {
				system_caret_ = true;
			}
		}
	}
	/* Restore the positions of the IME windows. */
	UpdateImeWindow(window_handle);
}


void GHOST_ImeWin32::SetImeWindowStyle(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam, BOOL *handled)
{
	/**
	 * To prevent the IMM (Input Method Manager) from displaying the IME
	 * composition window, Update the styles of the IME windows and EXPLICITLY
	 * call ::DefWindowProc() here.
	 * NOTE(hbono): We can NEVER let WTL call ::DefWindowProc() when we update
	 * the styles of IME windows because the 'lparam' variable is a local one
	 * and all its updates disappear in returning from this function, i.e. WTL
	 * does not call ::DefWindowProc() with our updated 'lparam' value but call
	 * the function with its original value and over-writes our window styles.
	 */
	*handled = TRUE;
	lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
	::DefWindowProc(window_handle, message, wparam, lparam);
}


void GHOST_ImeWin32::DestroyImeWindow(HWND window_handle)
{
	/* Destroy the system caret if we have created for this IME input context. */
	if (system_caret_) {
		::DestroyCaret();
		system_caret_ = false;
	}
}


void GHOST_ImeWin32::MoveImeWindow(HWND window_handle, HIMC imm_context)
{
	int x = caret_rect_.m_l;
	int y = caret_rect_.m_t;
	const int kCaretMargin = 1;
	/**
	 * As written in a comment in GHOST_ImeWin32::CreateImeWindow(),
	 * Chinese IMEs ignore function calls to ::ImmSetCandidateWindow()
	 * when a user disables TSF (Text Service Framework) and CUAS (Cicero
	 * Unaware Application Support).
	 * On the other hand, when a user enables TSF and CUAS, Chinese IMEs
	 * ignore the position of the current system caret and uses the
	 * parameters given to ::ImmSetCandidateWindow() with its 'dwStyle'
	 * parameter CFS_CANDIDATEPOS.
	 * Therefore, we do not only call ::ImmSetCandidateWindow() but also
	 * set the positions of the temporary system caret if it exists.
	 */
	CANDIDATEFORM candidate_position = { 0, CFS_CANDIDATEPOS, { x, y },
	{ 0, 0, 0, 0 } };
	::ImmSetCandidateWindow(imm_context, &candidate_position);
	if (system_caret_) {
		switch (PRIMARYLANGID(input_language_id_)) {
		case LANG_JAPANESE:
			::SetCaretPos(x, y + caret_rect_.getHeight());
			break;
		default:
			::SetCaretPos(x, y);
			break;
		}
	}
	if (PRIMARYLANGID(input_language_id_) == LANG_KOREAN) {
		/**
		 * Chinese IMEs and Japanese IMEs require the upper-left corner of
		 * the caret to move the position of their candidate windows.
		 * On the other hand, Korean IMEs require the lower-left corner of the
		 * caret to move their candidate windows.
		 */
		y += kCaretMargin;
	}
	/**
	 * Japanese IMEs and Korean IMEs also use the rectangle given to
	 * ::ImmSetCandidateWindow() with its 'dwStyle' parameter CFS_EXCLUDE
	 * to move their candidate windows when a user disables TSF and CUAS.
	 * Therefore, we also set this parameter here.
	 */
	CANDIDATEFORM exclude_rectangle = { 0, CFS_EXCLUDE, { x, y },
	{ x, y, x + caret_rect_.getWidth(), y + caret_rect_.getHeight() } };
	::ImmSetCandidateWindow(imm_context, &exclude_rectangle);
}


void GHOST_ImeWin32::UpdateImeWindow(HWND window_handle)
{
	/* Just move the IME window attached to the given window. */
	if (caret_rect_.m_l >= 0 && caret_rect_.m_t >= 0) {
		HIMC imm_context = ::ImmGetContext(window_handle);
		if (imm_context) {
			MoveImeWindow(window_handle, imm_context);
			::ImmReleaseContext(window_handle, imm_context);
		}
	}
}


void GHOST_ImeWin32::CleanupComposition(HWND window_handle)
{
	/**
	 * Notify the IMM attached to the given window to complete the ongoing
	 * composition, (this case happens when the given window is de-activated
	 * while composing a text and re-activated), and reset the omposition status.
	 */
	if (is_composing_) {
		HIMC imm_context = ::ImmGetContext(window_handle);
		if (imm_context) {
			::ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
			::ImmReleaseContext(window_handle, imm_context);
		}
		ResetComposition(window_handle);
	}
}


void GHOST_ImeWin32::CheckFirst(HWND window_handle)
{
	if (is_first) {
		this->EndIME(window_handle);
		is_first = false;
	}
}


void GHOST_ImeWin32::ResetComposition(HWND window_handle)
{
	/* Currently, just reset the composition status. */
	is_composing_ = false;
}


void GHOST_ImeWin32::CompleteComposition(HWND window_handle, HIMC imm_context)
{
	/**
	 * We have to confirm there is an ongoing composition before completing it.
	 * This is for preventing some IMEs from getting confused while completing an
	 * ongoing composition even if they do not have any ongoing compositions.)
	 */
	if (is_composing_) {
		::ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
		ResetComposition(window_handle);
	}
}


void GHOST_ImeWin32::GetCaret(HIMC imm_context, LPARAM lparam, ImeComposition *composition)
{
	/**
	 * This operation is optional and language-dependent because the caret
	 * style is depended on the language, e.g.:
	 *   * Korean IMEs: the caret is a blinking block,
	 *     (It contains only one hangul character);
	 *   * Chinese IMEs: the caret is a blinking line,
	 *     (i.e. they do not need to retrieve the target selection);
	 *   * Japanese IMEs: the caret is a selection (or underlined) block,
	 *     (which can contain one or more Japanese characters).
	 */
	int target_start = -1;
	int target_end = -1;
	switch (PRIMARYLANGID(input_language_id_)) {
	case LANG_KOREAN:
		if (lparam & CS_NOMOVECARET) {
			target_start = 0;
			target_end = 1;
		}
		break;
	case LANG_CHINESE:
	{
		int clause_size = ImmGetCompositionStringW(imm_context, GCS_COMPCLAUSE, NULL, 0);
		if (clause_size) {
			static std::vector<unsigned long> clauses;
			clause_size = clause_size / sizeof(clauses[0]);
			clauses.resize(clause_size);
			ImmGetCompositionStringW(imm_context, GCS_COMPCLAUSE, &clauses[0],
				sizeof(clauses[0])  *clause_size);
			if (composition->cursor_position == composition->ime_string.size()) {
				target_start = clauses[clause_size - 2];
				target_end = clauses[clause_size - 1];
			}
			else {
				for (int i = 0; i < clause_size - 1; i++) {
					if (clauses[i] == composition->cursor_position) {
						target_start = clauses[i];
						target_end = clauses[i + 1];
						break;
					}
				}
			}
		}
		else {
			if (composition->cursor_position != -1) {
				target_start = composition->cursor_position;
				target_end = composition->ime_string.size();
			}
		}
		break;
	}
	case LANG_JAPANESE:

		/**
		 * For Japanese IMEs, the robustest way to retrieve the caret
		 * is scanning the attribute of the latest composition string and
		 * retrieving the begining and the end of the target clause, i.e.
		 * a clause being converted.
		 */
		if (lparam & GCS_COMPATTR) {
			int attribute_size = ::ImmGetCompositionStringW(imm_context,
				GCS_COMPATTR,
				NULL, 0);
			if (attribute_size > 0) {
				char *attribute_data = new char[attribute_size];
				if (attribute_data) {
					::ImmGetCompositionStringW(imm_context, GCS_COMPATTR,
						attribute_data, attribute_size);
					for (target_start = 0; target_start < attribute_size;
						++target_start) {
						if (IsTargetAttribute(attribute_data[target_start]))
							break;
					}
					for (target_end = target_start; target_end < attribute_size;
						++target_end) {
						if (!IsTargetAttribute(attribute_data[target_end]))
							break;
					}
					if (target_start == attribute_size) {
						/**
						 * This composition clause does not contain any target clauses,
						 * i.e. this clauses is an input clause.
						 * We treat whole this clause as a target clause.
						 */
						target_end = target_start;
						target_start = 0;
					}
					if (target_start != -1 && target_start < attribute_size &&
						attribute_data[target_start] == ATTR_TARGET_NOTCONVERTED)
					{
						composition->cursor_position = target_start;
					}
				}
				delete[] attribute_data;
			}
		}
		break;
	}
	composition->target_start = target_start;
	composition->target_end = target_end;
}


bool GHOST_ImeWin32::GetString(HIMC imm_context, WPARAM lparam, int type, ImeComposition *composition)
{
	bool result = false;
	if (lparam & type) {
		int string_size = ::ImmGetCompositionStringW(imm_context, type, NULL, 0);
		if (string_size > 0) {
			int string_length = string_size / sizeof(wchar_t);
			wchar_t *string_data = new wchar_t[string_length + 1];
			string_data[string_length] = '\0';
			if (string_data) {
				/* Fill the given ImeComposition object. */
				::ImmGetCompositionStringW(imm_context, type,
					string_data, string_size);
				composition->string_type = type;
				composition->ime_string = string_data;
				result = true;
			}
			delete[] string_data;
		}
	}
	return result;
}


bool GHOST_ImeWin32::GetResult(HWND window_handle, LPARAM lparam, ImeComposition *composition)
{
	bool result = false;
	HIMC imm_context = ::ImmGetContext(window_handle);
	if (imm_context) {
		/* Copy the result string to the ImeComposition object. */
		result = GetString(imm_context, lparam, GCS_RESULTSTR, composition);
		/**
		 * Reset all the other parameters because a result string does not
		 * have composition attributes.
		 */
		composition->cursor_position = -1;
		composition->target_start = -1;
		composition->target_end = -1;
		::ImmReleaseContext(window_handle, imm_context);
	}
	return result;
}


bool GHOST_ImeWin32::GetComposition(HWND window_handle, LPARAM lparam, ImeComposition *composition)
{
	bool result = false;
	HIMC imm_context = ::ImmGetContext(window_handle);
	if (imm_context) {
		/* Copy the composition string to the ImeComposition object. */
		result = GetString(imm_context, lparam, GCS_COMPSTR, composition);

		/* Retrieve the cursor position in the IME composition. */
		int cursor_position = ::ImmGetCompositionStringW(imm_context, GCS_CURSORPOS, NULL, 0);
		composition->cursor_position = cursor_position;
		composition->target_start = -1;
		composition->target_end = -1;

		/* Retrieve the target selection and Update the ImeComposition object. */
		GetCaret(imm_context, lparam, composition);

		/* Mark that there is an ongoing composition. */
		is_composing_ = true;

		::ImmReleaseContext(window_handle, imm_context);
	}
	return result;
}


void GHOST_ImeWin32::EndIME(HWND window_handle)
{
	/**
	 * A renderer process have moved its input focus to a password input
	 * when there is an ongoing composition, e.g. a user has clicked a
	 * mouse button and selected a password input while composing a text.
	 * For this case, we have to complete the ongoing composition and
	 * clean up the resources attached to this object BEFORE DISABLING THE IME.
	 */
	if (!is_enable) return;
	is_enable = false;
	CleanupComposition(window_handle);
	::ImmAssociateContextEx(window_handle, NULL, 0);
	eventImeData.composite_len = 0;
}


void GHOST_ImeWin32::BeginIME(HWND window_handle, const GHOST_Rect &caret_rect, bool complete)
{
	if (is_enable && complete) return;
	is_enable = true;
	/**
	 * Load the default IME context.
	 * NOTE(hbono)
	 *   IMM ignores this call if the IME context is loaded. Therefore, we do
	 *   not have to check whether or not the IME context is loaded.
	 */
	::ImmAssociateContextEx(window_handle, NULL, IACE_DEFAULT);
	/* Complete the ongoing composition and move the IME windows. */
	HIMC imm_context = ::ImmGetContext(window_handle);
	if (imm_context) {
		if (complete) {
			/**
			 * A renderer process have moved its input focus to another edit
			 * control when there is an ongoing composition, e.g. a user has
			 * clicked a mouse button and selected another edit control while
			 * composing a text.
			 * For this case, we have to complete the ongoing composition and
			 * hide the IME windows BEFORE MOVING THEM.
			 */
			CompleteComposition(window_handle, imm_context);
		}
		/**
		 * Save the caret position, and Update the position of the IME window.
		 * This update is used for moving an IME window when a renderer process
		 * resize/moves the input caret.
		 */
		if (caret_rect.m_l >= 0 && caret_rect.m_t >= 0) {
			caret_rect_ = caret_rect;
			MoveImeWindow(window_handle, imm_context);
		}
		::ImmReleaseContext(window_handle, imm_context);
	}
}


static void convert_utf16_to_utf8_len(std::wstring s, int &len)
{
	if (len >= 0 && len <= s.size())
		len = count_utf_8_from_16(s.substr(0, len).c_str()) - 1;
	else
		len = -1;
}


static size_t updateUtf8Buf(ImeComposition &info)
{
	size_t len = count_utf_8_from_16(info.ime_string.c_str());
	info.utf8_buf.resize(len);
	conv_utf_16_to_8(info.ime_string.c_str(), &info.utf8_buf[0], len);
	convert_utf16_to_utf8_len(info.ime_string, info.cursor_position);
	convert_utf16_to_utf8_len(info.ime_string, info.target_start);
	convert_utf16_to_utf8_len(info.ime_string, info.target_end);
	return len - 1;
}


void GHOST_ImeWin32::UpdateInfo(HWND window_handle)
{
	int res = this->GetResult(window_handle, GCS_RESULTSTR, &resultInfo);
	int comp = this->GetComposition(window_handle, GCS_COMPSTR | GCS_COMPATTR, &compInfo);
	/* convert wchar to utf8 */
	if (res) {
		eventImeData.result_len = (GHOST_TUserDataPtr)updateUtf8Buf(resultInfo);
		eventImeData.result = &resultInfo.utf8_buf[0];
	}
	else {
		eventImeData.result = 0;
		eventImeData.result_len = 0;
	}
	if (comp) {
		eventImeData.composite_len = (GHOST_TUserDataPtr)updateUtf8Buf(compInfo);
		eventImeData.composite = &compInfo.utf8_buf[0];
		eventImeData.cursor_position = compInfo.cursor_position;
		eventImeData.target_start = compInfo.target_start;
		eventImeData.target_end = compInfo.target_end;
	}
	else {
		eventImeData.composite = 0;
		eventImeData.composite_len = 0;
		eventImeData.cursor_position = -1;
		eventImeData.target_start = -1;
		eventImeData.target_end = -1;
	}
}
