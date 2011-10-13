Blender translation HOWTO
=========================

I'll try briefly explain how translation works and how to update translation files.

1. How it works
---------------

This folder contains source files for translation system. These source files have
got .po extension and they've got pretty simple syntax:

msgid "some message id"
msgstr "translation for this message"

This means when string "some message id" is used as operator name, tooltip, menu
and so it'll be displayed on the screen as "translation for this message".
Pretty simple.

This source files are pre-compiled into ../release/bin/.blender/locale/<language>/LC_MESSAGES/blender.mo,
so they aren't getting compiled every time Blender is compiling to save some time and prevent
failure on systems which don't have needed tools for compiling .po files.

2. How to update translations
-----------------------------

It's also pretty simple. If you can find string you want to translate in <language>.po
file as msgid, just write correct msgstr string for it. If msgid is marked as fuzzy,
i.e.

#, fuzzy
msgid "some message id"
msgstr "translation for this message"

it means translation used to exist for this message, but message was changed, so translation
also have to be updated (it's easier to make new translation based on previous translation).
When translation was updated, remove line with '#, fuzzy' and it'll work.

If there's no message in .po file you want to translate, probably .po file should be updated.
Use the following steps for this:
- With newly compiled blender run:
  `blender --background --factory-startup --python update_msg.py`
  to update messages.txt file (this file contains strings collected
  automatically from RNA system and python UI scripts)
- Run update_pot.py script which will update blender.pot file. This file contains all
  strings which should be transated.
- Run update_po.py script to merge all .po files with blender.pot (so all .po files
  will contain all msgid-s declared in blender.pot) or update_po.py <language> to
  update only needed .po file(s) to save time when you're busy with translation.
  But before commit all .po files better be updated.

When you've finished with translation, you should re-compile .po file into .mo file.
It's also pretty simple: just run update_mo.py script to recompile all languages or
just update_mo.py <language> to re-compile only needed language(s).

NOTE: msgfmt, msgmerge and xgettext tools should be available in your PATH.

These steps to update template, translation files and compile them can be made in "batch" mode
using GNUMakefile:

make -f GNUMakefile translations

NOTE: Blender has to be compiled using GNUMakefile first.


3. Note for Windows users
-------------------------
You can find compiled builds of gettext in the lib folder under "binaries\gettext\" for both windows and win64.
In order to run the scripts you will need to replace the location of the GETTEXT_..._EXECUTABLE.

For example in update_pot.py:
-GETTEXT_XGETTEXT_EXECUTABLE = "xgettext"
+GETTEXT_XGETTEXT_EXECUTABLE = "C:\\Blender\\lib\\\windows\\\binaries\\\gettext\\xgettext.exe"

4. Other scripts
----------------

- check_po.py: this script checks if all messages declared in blender.pot exists in.po files
               and that no extra messages are declared in .po files
- clean_po.py: this script removes all commented messages which aren't required by .pot file anymore.
- merge_po.py: this script accepts two files as arguments and copies translations from second file
               into first file.
