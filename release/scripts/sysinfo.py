#!BPY
"""
Name: 'System Info'
Blender: 233
Group: 'Utils'
Tooltip: 'Information about your Blender environment, useful to diagnose problems.'
"""

# $Id$
#
# --------------------------------------------------------------------------
# sysinfo.py version 0.1 Jun 09, 2004
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004: Willian P. Germano, wgermano _at_ ig.com.br
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

import Blender
from Blender.BGL import *
import sys

# has_textwrap = 1 # see commented code below
output_filename = "system-info.txt"
warnings = 0

def cutPoint(text, length):
  "Returns position of the last space found before 'length' chars"
  l = length
  c = text[l]
  while c != ' ':
    l -= 1
    if l == 0: return length # no space found
    c = text[l]
  return l

def textWrap(text, length = 70):
  lines = []
  while len(text) > 70:
    cpt = cutPoint(text, length)
    line, text = text[:cpt], text[cpt + 1:]
    lines.append(line)
  lines.append(text)
  return lines

## Better use our own text wrap functions here
#try:
#  import textwrap
#except:
#  has_textwrap = 0
#  msg = sys.exc_info()[1].__str__().split()[3]
#  Blender.Draw.PupMenu("Python error:|This script requires the %s module" %msg)

header = "=  Blender %s System Information  =" % Blender.Get("version")
lilies = len(header)*"="+"\n"
header = lilies + header + "\n" + lilies

output = Blender.Text.New(output_filename)

output.write(header + "\n\n")

output.write("Platform: %s\n========\n\n" % sys.platform)

output.write("Python:\n======\n\n")
output.write("- Version: %s\n\n" % sys.version)
output.write("- Path:\n\n")
for p in sys.path:
  output.write(p + '\n')

output.write("\n- Default folder for registered scripts:\n\n")
scriptsdir = Blender.Get("datadir")
if scriptsdir:
  scriptsdir = scriptsdir.replace("/bpydata","/scripts")
  output.write(scriptsdir)
else:
  output.write("<WARNING> -- not found")
  warnings += 1

missing_mods = [] # missing basic modules

try:
  from mod_blender import basic_modules
  for m in basic_modules:
    try: exec ("import %s" % m)
    except: missing_mods.append(m)

  if missing_mods:
    output.write("\n\n<WARNING>:\n\nSome expected modules were not found.\n")
    output.write("Because of that some scripts bundled with Blender may not work.\n")
    output.write("Please read the FAQ in the Readme.html file shipped with Blender\n")
    output.write("for information about how to fix the problem.\n\n") 
    output.write("The missing modules:\n")
    warnings += 1
    for m in missing_mods:
      output.write('-> ' + m + '\n')
  else:
    output.write("\n\n- Modules: all basic ones were found.\n")

except:
  output.write("\n\n<WARNING>:\nCouldn't find mod_blender.py in scripts dir.")
  output.write("\nBasic modules availability won't be tested.\n")
  warnings += 1


output.write("\nOpenGL:\n======\n\n")
output.write("- Renderer: %s\n" % glGetString(GL_RENDERER))
output.write("- Vendor:   %s\n" % glGetString(GL_VENDOR))
output.write("- Version:  %s\n\n" % glGetString(GL_VERSION))
output.write("- Extensions:\n\n")

glext = glGetString(GL_EXTENSIONS)
glext = textWrap(glext, 70)

for l in glext:
  output.write(l + "\n")

output.write("\n\n- Simplistic almost useless benchmark:\n\n")
t = Blender.sys.time()
nredraws = 10
for i in range(nredraws):
  Blender.Redraw(-1) # redraw all windows
result = str(Blender.sys.time() - t)
output.write("Redrawing all areas %s times took %s seconds.\n" % (nredraws, result))

if (warnings):
  output.write("\n(*) Found %d warning" % warnings)
  if (warnings > 1): output.write("s") # (blush)
  output.write(", documented in the text above.")
else: output.write("\n==\nNo problems were found.")

exitmsg = "Done!|Please check the text %s at the Text Editor window." % output.name
Blender.Draw.PupMenu(exitmsg)
