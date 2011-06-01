#!/usr/bin/python

#update all po files in the LANGS

import os

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
    # update po file
    cmd = "msgmerge --update --lang=%s %s.po %s.pot" % (lang, lang, DOMAIN)
    print(cmd)
    os.system( cmd )

    # show stats
    cmd = "msgfmt --statistics %s.po" % lang
    os.system( cmd )
