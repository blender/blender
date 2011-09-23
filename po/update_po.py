#!/usr/bin/python

# update all po files in the LANGS

import os

PO_DIR = "."
DOMAIN = "blender"

for po in os.listdir( PO_DIR ):
  if po.endswith(".po"):
    lang = po[:-3]
    # update po file
    cmd = "msgmerge --update --lang=%s %s.po %s.pot" % (lang, lang, DOMAIN)
    print(cmd)
    os.system( cmd )
    
