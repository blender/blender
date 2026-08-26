package org.libsdl.app;

import android.content.*;
import android.os.Build;
import android.text.Editable;
import android.view.*;
import android.view.inputmethod.BaseInputConnection;
import android.widget.EditText;

class SDLInputConnection extends BaseInputConnection
{
    protected EditText mEditText;
    protected String mCommittedText = "";

    SDLInputConnection(View targetView, boolean fullEditor) {
        super(targetView, fullEditor);
        mEditText = new EditText(SDL.getContext());
    }

    @Override
    public Editable getEditable() {
        return mEditText.getEditableText();
    }

    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        /*
         * This used to handle the keycodes from soft keyboard (and IME-translated input from hardkeyboard)
         * However, as of Ice Cream Sandwich and later, almost all soft keyboard doesn't generate key presses
         * and so we need to generate them ourselves in commitText.  To avoid duplicates on the handful of keys
         * that still do, we empty this out.
         */

        /*
         * Return DOES still generate a key event, however.  So rather than using it as the 'click a button' key
         * as we do with physical keyboards, let's just use it to hide the keyboard.
         */

        if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
            if (SDLActivity.onNativeSoftReturnKey()) {
                return true;
            }
        }

        return super.sendKeyEvent(event);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        if (!super.commitText(text, newCursorPosition)) {
            return false;
        }
        updateText();
        return true;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        if (!super.setComposingText(text, newCursorPosition)) {
            return false;
        }
        updateText();
        return true;
    }

    @Override
    public boolean deleteSurroundingText(int beforeLength, int afterLength) {
        if (Build.VERSION.SDK_INT <= 29 /* Android 10.0 (Q) */) {
            // Workaround to capture backspace key. Ref: http://stackoverflow.com/questions>/14560344/android-backspace-in-webview-baseinputconnection
            // and https://bugzilla.libsdl.org/show_bug.cgi?id=2265
            if (beforeLength > 0 && afterLength == 0) {
                // backspace(s)
                while (beforeLength-- > 0) {
                    nativeGenerateScancodeForUnichar('\b');
                }
                return true;
           }
        }

        if (!super.deleteSurroundingText(beforeLength, afterLength)) {
            return false;
        }
        updateText();
        return true;
    }

    protected void updateText() {
        final Editable content = getEditable();
        if (content == null) {
            return;
        }

        String text = content.toString();
        int compareLength = Math.min(text.length(), mCommittedText.length());
        int matchLength, offset;

        /* Backspace over characters that are no longer in the string */
        for (matchLength = 0; matchLength < compareLength; ) {
            int codePoint = mCommittedText.codePointAt(matchLength);
            if (codePoint != text.codePointAt(matchLength)) {
                break;
            }
            matchLength += Character.charCount(codePoint);
        }
        /* FIXME: This doesn't handle graphemes, like '🌬️' */
        for (offset = matchLength; offset < mCommittedText.length(); ) {
            int codePoint = mCommittedText.codePointAt(offset);
            nativeGenerateScancodeForUnichar('\b');
            offset += Character.charCount(codePoint);
        }

        if (matchLength < text.length()) {
            String pendingText = text.subSequence(matchLength, text.length()).toString();
            if (!SDLActivity.dispatchingKeyEvent()) {
                for (offset = 0; offset < pendingText.length(); ) {
                    int codePoint = pendingText.codePointAt(offset);
                    if (codePoint == '\n') {
                        if (SDLActivity.onNativeSoftReturnKey()) {
                            return;
                        }
                    }
                    /* Higher code points don't generate simulated scancodes */
                    if (codePoint > 0 && codePoint < 128) {
                        nativeGenerateScancodeForUnichar((char)codePoint);
                    }
                    offset += Character.charCount(codePoint);
                }
            }
            SDLInputConnection.nativeCommitText(pendingText, 0);
        }
        mCommittedText = text;
    }

    public static native void nativeCommitText(String text, int newCursorPosition);

    public static native void nativeGenerateScancodeForUnichar(char c);
}

