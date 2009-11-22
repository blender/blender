# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy

def collect_baseclasses(_class, bases):

    if _class is type or _class is object:
        return bases

    bases.append(_class)
    for _superclass in _class.__bases__:
        collect_baseclasses(_superclass, bases)

    return bases

def collect_subclasses(_class, subs):

    if _class is type or _class is object:
        return subs

    subs.append(_class)
    for _subclass in _class.__subclasses__():
        collect_subclasses(_subclass, subs)

    return subs

class DynMenu(bpy.types.Menu):

    def draw(self, context):
        '''
        This is a draw function that is used to call all subclasses draw functions
        starting from the registered classes draw function and working down.

        DynMenu.setup() must be called first.

        Sort/group classes could be nice
        '''

        subclass_ls = []
        collect_subclasses(self.__class__, subclass_ls)
        # print(subclass_ls)

        for subclass in subclass_ls:
            # print("drawwing", subclass) # , dir(subclass))
            subclass.internal_draw(self, context)
            # print("subclass.internal_draw", subclass.internal_draw)

def setup(menu_class):
    '''
    Setup subclasses (not needed when self.add() is used)
    '''
    bases = collect_baseclasses(menu_class, [])

    # Incase 'DynMenu' isnt last
    while bases[-1] is not DynMenu:
        bases.pop()
    bases.pop() # remove 'DynMenu'

    root_class = bases[-1] # this is the registered class

    for subclass in collect_subclasses(root_class, []):
        #print(subclass)

        draw = getattr(subclass, 'draw', None)
        if draw and not hasattr(subclass, 'internal_draw'):
            # print("replace", subclass, draw)
            try:
                del subclass.draw
            except:
                pass
            subclass.internal_draw = draw

    root_class.draw = DynMenu.draw

def add(menu_class, func):
    '''
    Add a single function directly without having to make a class

    important that the returned value should be stored in the module that called it.
    '''

    newclass = type('<menuclass>', (menu_class,), {})
    newclass.internal_draw = func
    setup(menu_class)
    return newclass

'''
# so we dont need to import this module
DynMenu.setup = setup
DynMenu.add = add

# Only so we can access as bpy.types.
# dont ever use this directly!
bpy.types.register(DynMenu)
'''
