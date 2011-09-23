#!/usr/bin/python

# update all mo files in the LANGS

import os

LOCALE_DIR="../release/bin/.blender/locale"
PO_DIR = "."
DOMAIN = "blender"

for po in os.listdir( PO_DIR ):
  if po.endswith(".po"):
    lang = po[:-3]
    # show stats
    cmd = "msgfmt --statistics %s.po -o %s/%s/LC_MESSAGES/%s.mo" % ( lang, LOCALE_DIR, lang, DOMAIN )
    print cmd
    os.system( cmd )
