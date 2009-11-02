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

if 1:
    # Print once every 1000
    GEN_PATH = True
    PRINT_DATA = False
    PRINT_DATA_INT = 1000
    VERBOSE = False
    VERBOSE_TYPE = False
    MAX_RECURSIVE = 8
else:
    # Print everything
    GEN_PATH = True
    PRINT_DATA = True
    PRINT_DATA_INT = 0
    VERBOSE = False
    VERBOSE_TYPE = False
    MAX_RECURSIVE = 8

seek_count = [0]

def seek(r, txt, recurs):

    seek_count[0] += 1

    if PRINT_DATA_INT:
        if not (seek_count[0] % PRINT_DATA_INT):
            print(seek_count[0], txt)

    if PRINT_DATA:
        print(txt)

    newtxt = ''

    if recurs > MAX_RECURSIVE:
        #print ("Recursion is over max")
        #print (txt)
        return

    type_r = type(r)

    # print(type_r)
    # print(dir(r))

    # basic types
    if type_r in (float, int, bool, type(None)):
        if PRINT_DATA:
            print(txt + ' -> ' + str(r))
        return

    if type_r == str:
        if PRINT_DATA:
            print(txt + ' -> "' + str(r) + '"')
        return

    try:	keys = r.keys()
    except: keys = None

    if keys != None:
        if PRINT_DATA:
            print(txt + '.keys() - ' + str(r.keys()))

    try:	__members__ = dir(r)
    except: __members__ = []

    for item in __members__:
        if item.startswith('__'):
            continue

        if GEN_PATH: newtxt = txt + '.' + item

        if item == 'rna_type' and VERBOSE_TYPE==False: # just avoid because it spits out loads of data
            continue

        try:	value = getattr(r, item)
        except:	value = None

        seek( value, newtxt, recurs + 1)


    if keys:
        for k in keys:
            if GEN_PATH: newtxt = txt + '["' + k + '"]'
            seek(r.__getitem__(k), newtxt, recurs+1)

    else:
        try:	length = len( r )
        except:	length = 0

        if VERBOSE==False and length >= 4:
            for i in (0, length-1):
                if i>0:
                    if PRINT_DATA:
                        print((' '*len(txt)) + ' ... skipping '+str(length-2)+' items ...')

                if GEN_PATH: newtxt = txt + '[' + str(i) + ']'
                seek(r[i], newtxt, recurs+1)
        else:
            for i in range(length):
                if GEN_PATH: newtxt = txt + '[' + str(i) + ']'
                seek(r[i], newtxt, recurs+1)

seek(bpy.data, 'bpy.data', 0)
# seek(bpy.types, 'bpy.types', 0)
'''
for d in dir(bpy.types):
    t = getattr(bpy.types, d)
    try:	r = t.bl_rna
    except:	r = None
    if r:
        seek(r, 'bpy.types.' + d + '.bl_rna', 0)
'''

#print dir(bpy)
#import sys
#sys.exit()

print("iter over ", seek_count, "rna items")
