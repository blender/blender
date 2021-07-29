# -*- coding:utf-8 -*-

# #####BEGIN GPL LICENSE BLOCK #####
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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# #####END GPL LICENSE BLOCK #####

bl_info = {
  "name": "Material Library",
  "author": "Mackraken (mackraken2023@hotmail.com)",
  "version": (0, 5, 8),
  "blender": (2, 7, 8),
  "api": 60995,
  "location": "Properties > Material",
  "description": "Material Library VX",
  "warning": "",
  "wiki_url": "https://sites.google.com/site/aleonserra/home/scripts/matlib-vx",
  "tracker_url": "",
  "category": "Material"}


import bpy
import os
import json
from bpy.app.handlers import persistent
from bpy.props import (
    StringProperty, IntProperty, BoolProperty,
    PointerProperty, CollectionProperty
)
from bpy.types import (
    Panel, Menu, AddonPreferences, Operator,
    PropertyGroup,
    Scene
)


dev = False

matlib_path = os.path.dirname(__file__)

if dev:
  print (30*"-")
  matlib_path = r"D:\Blender Foundation\Blender\2.72\scripts\addons\matlib"

##debug print variables
def dd(*args, dodir=False):
  if dev:
    if dodir:
      print(dir(*args))
    print(*args)

#Regular Functions
def winpath(path):
  return path.replace("\\", "\\\\")

def update_search_index(self, context):
  search = self.search
  for i, it in enumerate(self.materials):
    if it.name==search:
      self.mat_index = i
      break

def check_path(path):
  #isabs sometimes returns true on relpaths
  if path and os.path.exists(path) and os.path.isfile(path) and os.path.isabs(path):
    try:
      if bpy.data.filepath and bpy.path.relpath(bpy.data.filepath) == bpy.path.relpath(path):
        return False
    except:
      pass
      #paths are on different drives. No problem then
    return True
  return False

def update_lib_index(self, context):
  self.load_library()

def update_cat_index(self, context):
  dd("cat index:", self.current_category, self.filter)

  if self.filter:
    self.filter = True


def update_filter(self, context):

  dd("filter:", self.filter, self.cat_index, self.current_category)
#  index = self.cat_index
#
#  if self.filter:
#    cat = self.current_category
#  else:
#    cat = ""
#
#  self.current_library.filter = cat
  self.update_list()

def check_index(collection, index):
  count = len(collection)
  return count>0 and index<count and index>=0

def send_command(cmd, output="sendmat.py"):
    bin = winpath(bpy.app.binary_path)
    scriptpath = winpath(os.path.join(bpy.app.tempdir, output))

    with open(scriptpath, "w") as f:
      f.write(cmd)

    import subprocess

    if output == "createlib.py":
      code = subprocess.call([bin, "-b", "-P", scriptpath])
    else:
      libpath = winpath(bpy.context.scene.matlib.current_library.path)
      code = subprocess.call([bin, "-b", libpath, "-P", scriptpath])

    #code returns 0 if ok, 1 if not
    return abs(code-1)

def list_materials(path, sort=False):
  list = []
  with bpy.data.libraries.load(path) as (data_from, data_to):
    for mat in data_from.materials:
      list.append(mat)

  if sort: list = sorted(list)
  return list

#category properties (none atm)
class EmptyGroup(PropertyGroup):
  pass
# bpy.utils.register_class(EmptyGroup)

class matlibMaterials(PropertyGroup):
  category = StringProperty()
# bpy.utils.register_class(matlibMaterials)

#bpy.types.Scene.matlib_categories = CollectionProperty(type=EmptyGroup)

### CATEGORIES
class Categories():

  #cats = bpy.context.scene.matlib.categories

  def __init__(self, cats):
    self.cats = cats

  def save(self):
    scn = bpy.context.scene
    cats = set([cat.name for cat in self.cats])
    libpath = bpy.context.scene.matlib.current_library.path

    cmd = """
print(30*"+")
import bpy
if not hasattr(bpy.context.scene, "matlib_categories"):
  class EmptyProps(bpy.types.PropertyGroup):
    pass
  bpy.utils.register_class(EmptyProps)
  bpy.types.Scene.matlib_categories = bpy.props.CollectionProperty(type=EmptyProps)
cats = bpy.context.scene.matlib_categories
for cat in cats:
  cats.remove(0)
"""
    for cat in cats:
      cmd += """
cat = cats.add()
cat.name = "%s" """ % cat.capitalize()
    cmd +='''
bpy.ops.wm.save_mainfile(filepath="%s", check_existing=False, compress=True)''' % winpath(libpath)

    return send_command(cmd, "save_categories.py")

  def read(self, pull=True):
    #mandar a imprimir el listado
    catfile = winpath(os.path.join(matlib_path, "categories.txt"))
    cmd = """
import bpy, json
class EmptyProps(bpy.types.PropertyGroup):
  pass
bpy.utils.register_class(EmptyProps)
bpy.types.Scene.matlib_categories = bpy.props.CollectionProperty(type=EmptyProps)
cats = []
for cat in bpy.context.scene.matlib_categories:
  materials = []
  for mat in bpy.data.materials:
    if "category" in mat.keys() and mat['category'] == cat.name:
      materials.append(mat.name)
  cats.append([cat.name, materials])
with open("%s", "w") as f:
  f.write(json.dumps(cats, sort_keys=True, indent=4))
""" % catfile
    if pull: send_command(cmd)

    #leer el fichero
    with open(catfile, "r") as f:
      cats = json.loads(f.read())

    dd(cats)

#    #refrescar categorias
#    for cat in self.cats:
#      self.cats.remove(0)
#
#    for cat in cats:
#      item = self.cats.add()
#      item.name = cat
#
    return cats

  def view(self):
    for cat in self.cats:
      dd(cat.name)

  def add(self, name):
    if name and name not in [item.name for item in self.cats]:
      name = name.strip().capitalize()
      item = self.cats.add()
      item.name = name
      if self.save():
        dd(name, "added")
        return True
    else:
      dd("duplicated?")

  def remove(self, index):
    self.cats.remove(index)
    self.save()

class Library():

  def __init__(self, matlib_path, name):
    self.name = name
    self.path = os.path.join(matlib_path, name)
#  @property
#  def default(self):
#    return self.name == default_library

  @property
  def shortname(self):
#    if self.default:
#      return "Default Library"
    return bpy.path.display_name(self.name).title()


  def __repr__(self):
    return str(type(self).__name__) + "('" + self.name + "')"

#bpy.utils.register_class(Library)

def get_libraries():
    libs = [Library(matlib_path, f) for f in os.listdir(matlib_path) if f[-5::] == "blend"]
    try:
        user_path = bpy.context.user_preferences.addons[__name__].preferences.matlib_path
        if os.path.exists(user_path):
            libs.extend([Library(user_path, f) for f in os.listdir(user_path) if f[-5::] == "blend"])
        else:
            print("path not found %s" % user_path)
    except:
        pass
    return sorted(libs, key=lambda x: bpy.path.display_name(x.name))

libraries = []
# get_libraries()

### MATLIB CLASS
class matlibProperties(PropertyGroup):

  #MATLIB PROPERTIES

  #libraries are read from the xml
  lib_index = IntProperty(min = -1, default = 2, update=update_lib_index)
  all_materials = CollectionProperty(type = matlibMaterials)
  materials = CollectionProperty(type = matlibMaterials)
  mat_index = IntProperty(min = -1, default = -1)
  categories = CollectionProperty(type = EmptyGroup)
  cat_index = IntProperty(min = -1, default = -1, update=update_cat_index)
  search = StringProperty(name="Search", description="Find By Name", update=update_search_index)

  #MATLIB OPTIONS
  #link: import material linked
  #force import:
  #   if disable it wont import a material if its present in the scene,(avoid duplicates)
  #   instead it will apply the scene material rather than importing the same one from the library
  #filter: enable or disable category filter
  #last selected: store the last selected object to regain focus when apply a material.
  #hide_search: Hides Search Field
  link = BoolProperty(name = "Linked", description="Link the material", default = False)
  force_import = BoolProperty(name = "Force Import", description="Use Scene Materials by default", default = False)
  filter = BoolProperty(name = "Filter",description="Filter Categories", default = False, update=update_filter)
  show_prefs = BoolProperty(name = "show_prefs", description="Preferences", default = False)
  last_selected = StringProperty(name="Last Selected")
  hide_search = BoolProperty(name="Hide Search", description="Use Blender Search Only")
  #import_file = StringProperty("Import File", subtype="FILE_PATH")
  #path = os.path.dirname(path)
  #Development only

  @property
  def libraries(self):
    global libraries
    return libraries

  @property
  def current_library(self):
    if check_index(libraries, self.lib_index):
      return libraries[self.lib_index]
  @property
  def active_material(self):
    if check_index(self.materials, self.mat_index):
      return self.materials[self.mat_index]

  def reload(self):
    dd("loading libraries")

    if self.current_library:
      self.load_library()
    elif self.lib_index == -1 and len(libraries):
      self.lib_index = 0


  def add_library(self, path, setEnabled = False):
    #sanitize path
    ext = os.path.extsep + "blend"
    if not path.endswith(ext):
      path += ext

    if check_path(path):
#      if path == default_library:
#        return 'ERROR', "Cannot add default library."
      #if path in [lib.path for lib in self.libraries]:
      return 'ERROR', "Library already exists."
    else:
      dd("Can't find " + path)
      #create file
      cmd = '''
import bpy
bpy.ops.wm.save_mainfile(filepath="%s", check_existing=False, compress=True)''' % winpath(path)
      if not (send_command(cmd, "createlib.py")):
        return 'ERROR', "There was an error creating the file. Make sure you run Blender with admin rights."

      #self.libraries = sorted(self.libraries, key=lambda lib: sortlibs(lib))
      dd("adding library", path)
      global libraries
      libraries = get_libraries()
      return "INFO", "Library added"

  def load_library(self):
    self.empty_list(True)
    if not self.current_library:
      return 'ERROR', "Library not found!."

    path = self.current_library.path

    dd("loading library", self.lib_index, path)

    if check_path(path):
      self.filter = False
      self.cat_index = -1

      categories = Categories(self.categories)
      self.cats = categories.read(True)
      self.load_categories()

      for mat in self.all_materials:
        self.all_materials.remove(0)

      for mat in list_materials(self.current_library.path, True):
        item = self.all_materials.add()
        item.name = mat
        for cat in self.cats:
          if mat in cat[1]:
            item.category = cat[0]
            break

      self.update_list()
    else:
      return 'ERROR', "Library not found!."

  def update_list(self):
    ### THIS HAS TO SORT
    self.empty_list()
    if self.current_library:
      current_category = self.current_category
      #sorteditems = sorted(self.all_materials, key=lambda x: x.name)
      for mat in self.all_materials:
        #print(current_category, mat.category)
        if not self.filter or (self.filter and mat.category == current_category) or current_category == "":
          item = self.materials.add()
          item.name = mat.name
          item.category = mat.category

  def empty_list(self, cats = False):
    #self.mat_index = -1
    for it in self.materials:
      self.materials.remove(0)

    if cats:
      for c in self.categories:
        self.categories.remove(0)

  ### CATEGORIES
  @property
  def current_category(self):
    #print(self.mat_index)
    if check_index(self.categories, self.cat_index):
      return self.categories[self.cat_index].name
    return ""

  def load_categories(self):

    for c in self.categories:
      self.categories.remove(0)

    for c in self.cats:
      cat = self.categories.add()
      cat.name = c[0]

  def add_category(self, name):
    if name:
      name = name.strip().title()
      dd("add category", name)
      categories = Categories(self.categories)

      categories.add(name)

#      if lib:
#        cat = xml.find("category", name, lib, create = True)
#        self.load_categories()
#      else:
#        return 'ERROR', "Library not found"
  def remove_category(self):
    dd("removing category", self.current_category)
    categories = Categories(self.categories)
    categories.remove(self.cat_index)

  def set_category(self):
    mat = self.active_material
    #dd(lib, mat, self.current_category)
    if mat:
      #set mat to category
      if self.cat_index>-1:
        dd(self.current_category)
        cat = self.current_category
        if cat == self.all_materials[self.mat_index].category:
          return
        cmd = """
import bpy
try:
  mat = bpy.data.materials['%s']
except:
  mat = None
if mat:
  mat['category'] = "%s"
  bpy.ops.wm.save_mainfile(filepath="%s", check_existing=False, compress=True)
""" % (mat.name, cat, winpath(self.current_library.path))
        if send_command(cmd):
          self.all_materials[self.mat_index].category = cat
          mat.category = cat
        else:
          return "WARNING", "There was an error."

#        catnode = xml.find("category", self.current_category, lib, True)
#        matnode = xml.find("material", mat.name, lib)
#        if matnode:
#          catnode.appendChild(matnode)
#        else:
#          matnode = xml.find("material", mat.name, catnode, True)
#        xml.save()
#        mat.category = cat
#        self.current_library.materials[self.mat_index].category = cat
      #remove mat from any category
      else:
        matnode = xml.find("material", mat.name, lib)
        if matnode:
          xml.deleteNode(matnode)
        mat.category = ""
        self.current_library.materials[self.mat_index].category = ""
    else:
      return "WARNING", "Select a material"

  def get_material(self, name, link=False):
    with bpy.data.libraries.load(self.current_library.path, link, False) as (data_from, data_to):
      data_to.materials = [name]
    if link:
      print(name + " linked.")
    else:
      print(name + " appended.")

  def apply(self, context, preview=False):
    name = self.active_material.name
    if not name: return "WARNING", "Select a material from the list."

    linked = self.link or preview
    force =  self.force_import or linked

    objects = []
    active = context.object
    dummy = self.get_dummy(context)

    #setup objects
    if preview:
      if context.mode == "EDIT_MESH":
        return "WARNING", "Can't preview on EDIT MODE"
      if dummy!= active:
        self.last_selected = context.object.name
      context.scene.objects.active = dummy
      objects.append(dummy)
    #apply
    else:
      objects = [obj for obj in context.selected_objects if hasattr(obj.data, "materials")]

    if not objects:
      return "INFO", "Please select an object"

    if dummy == context.object and not preview:
      if (len(objects)==1 and dummy.select):
        return "ERROR", "Apply is disabled for the Material Preview Object"
      try:
        last = context.scene.objects[self.last_selected]
        if last in context.selected_objects:
          context.scene.objects.active = last
        else:
          self.last_selected = ""
      except:
        context.scene.objects.active = None
    dummy.select = False
      #objects = context.selected_objects

    material = None

    #mira si hay materiales linkados de la libreria actual
    for mat in bpy.data.materials:
      try:
        samelib = bpy.path.relpath(mat.library.filepath) == bpy.path.relpath(self.current_library.path)
      except:
        samelib = False

      if mat.name == name and mat.library and samelib:
        material = mat
        dd("encontre linked", name, "no importo nada")
        break

    if not force:
      #busca materiales no linkados
      for mat in bpy.data.materials:
        if mat.name == name and not mat.library:
          material = mat
          dd("encontre no linkado", name, "no importo nada")
          break

    if not material:
      #go get it
      dd("voy a buscarlo")
      nmats = len(bpy.data.materials)

      self.get_material(name, linked)

      if not self.force_import:
        try:
          material = bpy.data.materials[name]
        except:
          pass

      if not material:
        if nmats == len(bpy.data.materials) and not linked:
          return "ERROR", name + " doesn't exists at library " + str(linked)
        else:
          for mat in reversed(bpy.data.materials):
            if mat.name[0:len(name)] == name:
              #careful on how blender writes library paths
              try:
                samelib = bpy.path.relpath(mat.library.filepath) == bpy.path.relpath(self.current_library.path)
              except:
                samelib = False

              if linked and mat.library and samelib:
                material = mat
                dd(name, "importado con link")
                break
              else:
                if not mat.library:
                  dd(name, "importado sin link")
                  material = mat
                  break
        if material:
          material.use_fake_user = False
          material.user_clear()

    print ("Material", material, force)

    #if material:
    #maybe some test cases doesnt return a material, gotta take care of that
    #i cannot think of any case like that right now
    #maybe import linked when the database isnt sync
    if context.mode == "EDIT_MESH":
      obj = context.object
      dd(material)
      index = -1
      for i, mat in enumerate(obj.data.materials):
        if mat == material:
          index = i
          break

      if index == -1:
        obj.data.materials.append(material)
        index = len(obj.data.materials)-1
      dd(index)
      import bmesh
      bm  = bmesh.from_edit_mesh(obj.data)
      for f in bm.faces:
        if f.select:
          f.material_index = index

    else:
      for obj in objects:
        index = obj.active_material_index
        if index < len(obj.material_slots):
          obj.material_slots[index].material = None
          obj.material_slots[index].material = material
        else:
          obj.data.materials.append(material)

      if not linked:
        bpy.ops.object.make_local(type="SELECT_OBDATA_MATERIAL")

  def add_material(self, mat):

    if not mat:
      return 'WARNING', "Select a material from the scene."

    name = mat.name
    thispath = winpath(bpy.data.filepath)
    libpath = winpath(self.current_library.path)

    if not thispath:
      return 'WARNING', "Save this file before export."

    if not libpath:
      return 'WARNING', "Library not found!."

    elif bpy.data.is_dirty:
      bpy.ops.wm.save_mainfile(check_existing=True)

    if mat.library:
      return 'WARNING', 'Cannot export linked materials.'

    dd("adding material", name, libpath)

    overwrite = ""
    if name in list_materials(libpath):
      overwrite = '''
mat = bpy.data.materials["%s"]
mat.name = "tmp"
mat.use_fake_user = False
mat.user_clear()'''  % name

    cmd = '''
import bpy{0}
with bpy.data.libraries.load("{1}") as (data_from, data_to):
  data_to.materials = ["{2}"]
mat = bpy.data.materials["{2}"]
mat.use_fake_user=True
bpy.ops.file.pack_all()
bpy.ops.wm.save_mainfile(filepath="{3}", check_existing=False, compress=True)
'''.format(overwrite, thispath, name, libpath)

    if send_command(cmd):
      #self.load_library()
      if not overwrite:
        item = self.all_materials.add()
        item.name = name
        if "category" in mat.keys():
          item.category = mat['category']
        #reorder all_materials
        items = sorted([[item.name, item.category] for item in self.all_materials], key = lambda x: x[0])

        self.all_materials.clear()
        for it in items:
          item = self.all_materials.add()
          item.name = it[0]
          item.category = it[1]

        self.update_list()

      return 'INFO', "Material added."
    else:
      print("Save Material Error: Run Blender with administrative priviledges.")
      return 'WARNING', "There was an error saving the material"

  def remove_material(self):
    name = self.active_material.name
    libpath = winpath(self.current_library.path)
    if name and libpath and name in list_materials(libpath):
      cmd = '''import bpy
mat = bpy.data.materials["%s"]
mat.use_fake_user = False
mat.user_clear()
bpy.ops.wm.save_mainfile(filepath="%s", check_existing=False, compress=True)''' % (name , libpath)
      if send_command(cmd, "removemat.py"):
        self.all_materials.remove(self.mat_index)
        self.update_list()
      else:
        return 'ERROR', "There was an error."
    return "INFO", name + " removed."

  def get_dummy(self, context):
    dummy_name = "Material_Preview_Dummy"
    dummy_mesh = "Material_Preview_Mesh"
    scn = context.scene
    try:
      dummy = scn.objects[dummy_name]
    except:
      #create dummy
      try:
        me = bpy.data.meshes(dummy_mesh)
      except:
        me = bpy.data.meshes.new(dummy_mesh)
      dummy = bpy.data.objects.new(dummy_name, me)
      scn.objects.link(dummy)

    dummy.hide = True
    dummy.hide_render = True
    dummy.hide_select = True
    return dummy

# bpy.utils.register_class(matlibProperties)
# Scene.matlib = PointerProperty(type = matlibProperties)

### MENUS
class matlibLibsMenu(Menu):
  bl_idname = "matlib.libs_menu"
  bl_label = "Libraries Menu"

  def draw(self, context):
    layout = self.layout
    libs = libraries
    #layout.operator("matlib.operator", text="Default Library").cmd="lib-1"
    for i, lib in enumerate(libs):
      layout.operator("matlib.operator", text=lib.shortname).cmd="lib"+str(i)

class matlibCatsMenu(Menu):
  bl_idname = "matlib.cats_menu"
  bl_label = "Categories Menu"

  def draw(self, context):
    layout = self.layout
    cats = context.scene.matlib.categories
    layout.operator("matlib.operator", text="All").cmd="cat-1"
    for i, cat in enumerate(cats):
      layout.operator("matlib.operator", text=cat.name).cmd="cat"+str(i)

### OPERATORS
#class MATLIB_OT_add(Operator):
#  """Add Active Material"""
#  bl_label = "Add"
#  bl_idname = "matlib.add_material"
#
#  @classmethod
#  def poll(cls, context):
#    return context.active_object is not None
#
#  def exectute(self, context):
#    print("executing")
#    return {"FINISHED"}



class MatlibAdd(Operator):
  """Add active material to library"""
  bl_idname = "matlib.add"
  bl_label = "Add active material"

  @classmethod
  def poll(cls, context):
    obj = context.active_object
    return  obj is not None and obj.active_material is not None

  def execute(self, context):
    matlib = context.scene.matlib
    success = matlib.add_material(context.object.active_material)
    if type(success).__name__ == "tuple":
      print(success)
      self.report({success[0]}, success[1])
    return {'FINISHED'}

class MatlibRemove(Operator):
  """Remove material from library"""
  bl_idname = "matlib.remove"
  bl_label = "Remove material from library"

  @classmethod
  def poll(cls, context):
    matlib = context.scene.matlib
    return check_index(matlib.materials, matlib.mat_index)

  def execute(self, context):
    matlib = context.scene.matlib
    success = matlib.remove_material()
    if type(success).__name__ == "tuple":
      print(success)
      self.report({success[0]}, success[1])
    return {'FINISHED'}

class MatlibReload(Operator):
  """Reload library"""
  bl_idname = "matlib.reload"
  bl_label = "Reload library"

#  @classmethod
#  def poll(cls, context):
#    matlib = context.scene.matlib
#    index = matlib.mat_index
#    l = len(matlib.materials)
#    return l>0 and index >=0 and index < l

  def execute(self, context):
    matlib = context.scene.matlib
    success = matlib.reload()
    if type(success).__name__ == "tuple":
      print(success)
      self.report({success[0]}, success[1])
    return {'FINISHED'}


class MatlibApply(Operator):
  """Apply selected material"""
  bl_idname = "matlib.apply"
  bl_label = "Apply material"

  @classmethod
  def poll(cls, context):
    matlib = context.scene.matlib
    index = matlib.mat_index
    l = len(matlib.materials)
    obj = context.active_object
    return l>0 and index >=0 and index < l and obj is not None

  def execute(self, context):
    matlib = context.scene.matlib
    success = matlib.apply(context, False)
    if type(success).__name__ == "tuple":
      print(success)
      self.report({success[0]}, success[1])
    return {'FINISHED'}


class MatlibPreview(Operator):
  """Preview selected material"""
  bl_idname = "matlib.preview"
  bl_label = "Preview selected material"

  @classmethod
  def poll(cls, context):
    matlib = context.scene.matlib
    index = matlib.mat_index
    l = len(matlib.materials)
    obj = context.active_object
    return l>0 and index >=0 and index < l

  def execute(self, context):
    matlib = context.scene.matlib
    success = matlib.apply(context, True)
    if type(success).__name__ == "tuple":
      print(success)
      self.report({success[0]}, success[1])
    return {'FINISHED'}


class MatlibFlush(Operator):
  """Flush unused materials"""
  bl_idname = "matlib.flush"
  bl_label = "Flush unused materials"

  @classmethod
  def poll(cls, context):
    matlib = context.scene.matlib
    index = matlib.mat_index
    l = len(matlib.materials)
    obj = context.active_object
    return l>0 and index >=0 and index < l

  def execute(self, context):
    matlib = context.scene.matlib
    dummy = matlib.get_dummy(context)
    if dummy == context.object:
      try:
        context.scene.objects.active = context.scene.objects[matlib.last_selected]
      except:
        pass

    for slot in dummy.material_slots:
      slot.material = None
    i=0
    for mat in bpy.data.materials:
      if mat.users==0:
        i+=1
        print (mat.name, "removed.")
        bpy.data.materials.remove(mat)

    plural = "" if i == 1 else "s"
    self.report({'INFO'}, str(i) + " material"+plural+" removed.")

    return {'FINISHED'}


class matlibOperator(Operator):
  """Add, Remove, Reload, Apply, Preview, Clean Material"""
  bl_label = "New"
  bl_idname = "matlib.operator"
  __doc__ = "Add, Remove, Reload, Apply, Preview, Clean Material"

  category = StringProperty(name="Category")
  filepath = StringProperty(options={'HIDDEN'})
  cmd = bpy.props.StringProperty(name="Command", options={'HIDDEN'})
  filter_glob = StringProperty(default="*.blend", options={'HIDDEN'})
  @classmethod
  def poll(cls, context):
    return context.active_object is not None

  def draw(self, context):
    layout = self.layout
    #cmd = LIBRARY_ADD
    if self.cmd == "LIBRARY_ADD":
      #layout.label("Select a blend file as library or")
      #layout.label("Type a name to create a new library.")
      layout.prop(self, "category", text="Library")
    elif self.cmd == "FILTER_ADD":
      layout.prop(self, "category")

  def invoke(self, context, event):

    cmd = self.cmd
    print("invoke", cmd)

    if cmd == "LIBRARY_ADD":
      self.filepath = matlib_path + os.path.sep
      dd("filepath", self.filepath, matlib_path)
      #context.window_manager.fileselect_add(self)
      context.window_manager.invoke_props_dialog(self)
      return {'RUNNING_MODAL'}
    elif cmd == "FILTER_ADD":
      context.window_manager.invoke_props_dialog(self)
      return {'RUNNING_MODAL'}
    return self.execute(context)

  ### TODO: execute doesnt trigger remove
  def execute(self, context):

    success = ""
    matlib = context.scene.matlib

    if self.cmd == "init":
      print("initialize")
      return {'FINISHED'}

    #Library Commands
    if self.cmd[0:3] == "lib":
      index = int(self.cmd[3::])
      matlib.lib_index = index
      #success = matlib.load_library()
    elif self.cmd == "LIBRARY_ADD":
      dd("execute lib add")
      libname = self.category
      if libname[-6::] != ".blend": libname+= ".blend"
      libname = os.path.join(matlib_path, libname)
      print(libname)

      success = matlib.add_library(libname, True)
      for i, l in enumerate(libraries):
        if l.name == self.category:
          matlib.lib_index = i
          break

    elif self.cmd == "RELOAD":
      success = matlib.reload()

    if not matlib.current_library:
      self.report({'ERROR'}, "Select a Library")
      return {'CANCELLED'}

    if self.cmd == "FILTER_ADD":
      success = matlib.add_category(self.category)
      for i, cat in enumerate(matlib.categories):
        if cat.name == self.category:
          matlib.cat_index = i
          break

    elif self.cmd == "FILTER_REMOVE":
      matlib.remove_category()

    elif self.cmd == "FILTER_SET":
      success = matlib.set_category()

    elif self.cmd[0:3] == "cat":
      index = int(self.cmd[3::])
      matlib.cat_index = index

    #Common Commands
    elif self.cmd == "ADD":
      success = matlib.add_material(context.object.active_material)

    elif self.cmd == "REMOVE":
      success = matlib.remove_material()


    elif self.cmd == "APPLY":
      success = matlib.apply(context)

    elif self.cmd == "PREVIEW":
      success = matlib.apply(context, True)

    elif self.cmd=="FLUSH":
      #release dummy materials
      dummy = matlib.get_dummy(context)
      if dummy == context.object:
        try:
          context.scene.objects.active = context.scene.objects[matlib.last_selected]
        except:
          pass

      for slot in dummy.material_slots:
        slot.material = None
      i=0
      for mat in bpy.data.materials:
        if mat.users==0:
          i+=1
          print (mat.name, "removed.")
          bpy.data.materials.remove(mat)

      plural = "s"
      if i==1:
        plural = ""

      self.report({'INFO'}, str(i) + " material"+plural+" removed.")

    ### CONVERT
    elif self.cmd == "CONVERT":
      return {'FINISHED'}
      lib = matlib.current_library
      if lib:

        path = os.path.join(matlib_path, "www")
        if not os.path.exists(path):
          os.mkdir(path)
        path = os.path.join(path, lib.shortname)
        if not os.path.exists(path):
          os.mkdir(path)

        path = winpath(path)
        libpath = winpath(lib.name)

        print(path)
        print(libpath)

        #decirle a la libreria que cree un fichero blend por cada material que tenga.
        cmd = """
print(30*"+")
import bpy, os
def list_materials():
  list = []
  with bpy.data.libraries.load("{0}") as (data_from, data_to):
    for mat in data_from.materials:
      list.append(mat)
  return sorted(list)

def get_material(name, link=False):
  with bpy.data.libraries.load("{0}", link, False) as (data_from, data_to):
    data_to.materials = [name]
  if link:
    print(name + " linked.")
  else:
    print(name + " appended.")

for scn in bpy.data.scenes:
  for obj in scn.objects:
    scn.objects.unlink(obj)
    obj.user_clear()
    bpy.data.objects.remove(obj)

def clean_materials():
  for mat in bpy.data.materials:
    mat.user_clear()
    bpy.data.materials.remove(mat)

bin = bpy.app.binary_path
mats = list_materials()
bpy.context.user_preferences.filepaths.save_version = 0
for mat in mats:
  clean_materials()
  matpath = os.path.join("{1}", mat + ".blend")
  print(matpath)
  get_material(mat)
  material = bpy.data.materials[0]
  material.use_fake_user = True
  bpy.ops.wm.save_mainfile(filepath = matpath, compress=True, check_existing=False)
""".format(libpath, path)
        print(cmd)
        send_command(cmd, "createlib.py")

    if type(success).__name__ == "tuple":
      print(success)
      self.report({success[0]}, success[1])

    return {'FINISHED'}


class matlibvxPanel(Panel):
  bl_label = "Material Library VX"
  bl_space_type = "PROPERTIES"
  bl_region_type = "WINDOW"
  bl_context = "material"

  @classmethod
  def poll(self, context):
    return context.active_object.active_material!=None

  def draw(self, context):
    layout = self.layout
    matlib = context.scene.matlib

    #hyper ugly trick but i dont know how to init classes at register time
#    if matlibProperties.init:
#      matlibProperties.init = False
#      matlib.__init__()

    #libaries
    row = layout.row(align=True)
    if matlib.current_library:
      text = matlib.current_library.shortname
    else:
      text = "Select a Library"

    row.menu("matlib.libs_menu",text=text)
    row.operator("matlib.operator", icon="ZOOMIN", text="").cmd = "LIBRARY_ADD"
    if matlib.active_material:
      row.label(matlib.active_material.category)
    else:
      row.label("")
#
#    #search
    if not matlib.hide_search:
      row = layout.row(align=True)
      row.prop_search(matlib, "search", matlib, "materials", text="", icon="VIEWZOOM")

#    #list
    row = layout.row()
    row.template_list("UI_UL_list", "  ", matlib, "materials", matlib, "mat_index", rows=6)
    col = row.column(align=True)
    row = layout.row()

      #operators
    col.operator("matlib.add", icon="ZOOMIN", text="")
    col.operator("matlib.remove", icon="ZOOMOUT", text="")
    col.operator("matlib.reload", icon="FILE_REFRESH", text="")
    col.operator("matlib.apply", icon="MATERIAL", text="")
    col.operator("matlib.preview", icon="COLOR", text="")
    col.operator("matlib.flush", icon="GHOST_DISABLED", text="")
    col.prop(matlib, "show_prefs", icon="MODIFIER", text="")

    #categories
    row = layout.row(align=True)
    text = "All"
    if matlib.current_category: text = matlib.current_category
    row.menu("matlib.cats_menu",text=text)
    row.prop(matlib, "filter", icon="FILTER", text="")
    row.operator("matlib.operator", icon="FILE_PARENT", text="").cmd="FILTER_SET"
    row.operator("matlib.operator", icon="ZOOMIN", text="").cmd="FILTER_ADD"
    row.operator("matlib.operator", icon="ZOOMOUT", text="").cmd="FILTER_REMOVE"

    #prefs
    if matlib.show_prefs:
      row = layout.row()
      row.prop(matlib, "force_import")
      row.prop(matlib, "link")
      row = layout.row()
      row.prop(matlib, "hide_search")
#      row = layout.row(align=True)
      #row = layout.row()
      #row.operator("matlib.operator", icon="URL", text="Convert Library").cmd="CONVERT"

#      row = layout.row()
#      if (matlib.current_library):
#        row.label(matlib.current_library.name)
#      else:
#        row.label("Library not found!.")

#classes = [matlibvxPanel, matlibOperator, matlibLibsMenu, matlibCatsMenu]
#print(bpy.context.scene)


@persistent
def refresh_libs(dummy=None):
    global libraries
    libraries = get_libraries()


def reload_library(self, context):
    refresh_libs(self)


class matlibvxPref(AddonPreferences):
    bl_idname = __name__

    matlib_path = StringProperty(
        name="Additional Path",
        description="User defined path to .blend libraries files",
        default="",
        update=reload_library
    )

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "matlib_path")


def register():
    global libraries
    bpy.utils.register_module(__name__)
    #  for c in classes:
    #    bpy.utils.register_class(c)
    Scene.matlib_categories = CollectionProperty(type=EmptyGroup)
    Scene.matlib = PointerProperty(type = matlibProperties)
    bpy.app.handlers.load_post.append(refresh_libs)
    libraries = get_libraries()


def unregister():
    global libraries
    bpy.utils.unregister_module(__name__)
    try:
        # raise ValueError list.remove(x): x not in list
        del Scene.matlib_categories
    except:
        pass
    del Scene.matlib
    libraries.clear()
    bpy.app.handlers.load_post.remove(refresh_libs)
    # for c in classes:
    #   bpy.utils.unregister_class(c)


if __name__ == "__main__":
    register()
