 # ***** BEGIN GPL LICENSE BLOCK *****
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
 # Contributor(s): Campbell Barton
 #
 # #**** END GPL LICENSE BLOCK #****

defs = """
    SPACE_EMPTY,
    SPACE_VIEW3D,
    SPACE_IPO,
    SPACE_OUTLINER,
    SPACE_BUTS,
    SPACE_FILE,
    SPACE_IMAGE,
    SPACE_INFO,
    SPACE_SEQ,
    SPACE_TEXT,
    SPACE_IMASEL,
    SPACE_SOUND,
    SPACE_ACTION,
    SPACE_NLA,
    SPACE_SCRIPT,
    SPACE_TIME,
    SPACE_NODE,
    SPACEICONMAX
"""

print '\tmod = PyModule_New("dummy");'
print '\tPyModule_AddObject( submodule, "key", mod );'

for d in defs.split('\n'):

    d = d.replace(',', ' ')
    w = d.split()

    if not w:
        continue

    try:		w.remove("#define")
    except:	pass

    # print w

    val = w[0]
    py_val = w[0]

    print '\tPyModule_AddObject( mod, "%s", PyLong_FromSize_t(%s) );' % (val, py_val)




