#!/usr/bin/python 

import os
import string
import sys

name = string.replace(sys.argv[1], ".zip", "")

os.system("rm -f %s.dmg" % (name))
os.system("mkdir -p /tmp/cycles_dmg")
os.system("rm /tmp/cycles_dmg/*")
os.system("cp %s.zip /tmp/cycles_dmg/" % (name))
os.system("/usr/bin/hdiutil create -fs HFS+ -srcfolder /tmp/cycles_dmg -volname %s %s.dmg" % (name, name))

