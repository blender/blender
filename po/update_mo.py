#!/usr/bin/python

#update all mo files in the LANGS

import os

LOCALE_DIR="../release/bin/.blender/locale"
DOMAIN = "blender"
LANGS = (
  "ar",
  "bg",
  "ca",
  "cs",
  "de",
  "el",
  "es",
  "fi",
  "fr",
  "hr",
  "it",
  "ja",
  "ko",
  "nl",
  "pl",
  "pt_BR",
  "ro",
  "ru",
  "sr@Latn",
  "sr",
  "sv",
  "uk",
  "zh_CN"
)

#-o %s.new.po
for lang in LANGS:
    # show stats
    cmd = "msgfmt --statistics %s.po -o %s/%s/LC_MESSAGES/%s.mo" % ( lang, LOCALE_DIR, lang, DOMAIN )
    print cmd
    os.system( cmd )
