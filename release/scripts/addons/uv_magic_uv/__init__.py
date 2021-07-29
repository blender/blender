# <pep8-80 compliant>

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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

__author__ = "Nutti <nutti.metro@gmail.com>"
__status__ = "production"
__version__ = "4.4"
__date__ = "2 Aug 2017"


bl_info = {
    "name": "Magic UV",
    "author": "Nutti, Mifth, Jace Priester, kgeogeo, mem, "
              "Keith (Wahooney) Boshoff, McBuff, MaxRobinot",
    "version": (4, 4, 0),
    "blender": (2, 79, 0),
    "location": "See Add-ons Preferences",
    "description": "UV Manipulator Tools. See Add-ons Preferences for details",
    "warning": "",
    "support": "COMMUNITY",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/UV/Magic_UV",
    "tracker_url": "https://github.com/nutti/Magic-UV",
    "category": "UV"
}

if "bpy" in locals():
    import importlib
    importlib.reload(muv_preferences)
    importlib.reload(muv_menu)
    importlib.reload(muv_common)
    importlib.reload(muv_props)
    importlib.reload(muv_cpuv_ops)
    importlib.reload(muv_cpuv_selseq_ops)
    importlib.reload(muv_fliprot_ops)
    importlib.reload(muv_transuv_ops)
    importlib.reload(muv_uvbb_ops)
    importlib.reload(muv_mvuv_ops)
    importlib.reload(muv_texproj_ops)
    importlib.reload(muv_packuv_ops)
    importlib.reload(muv_texlock_ops)
    importlib.reload(muv_mirroruv_ops)
    importlib.reload(muv_wsuv_ops)
    importlib.reload(muv_unwrapconst_ops)
    importlib.reload(muv_preserve_uv_aspect)
else:
    from . import muv_preferences
    from . import muv_menu
    from . import muv_common
    from . import muv_props
    from . import muv_cpuv_ops
    from . import muv_cpuv_selseq_ops
    from . import muv_fliprot_ops
    from . import muv_transuv_ops
    from . import muv_uvbb_ops
    from . import muv_mvuv_ops
    from . import muv_texproj_ops
    from . import muv_packuv_ops
    from . import muv_texlock_ops
    from . import muv_mirroruv_ops
    from . import muv_wsuv_ops
    from . import muv_unwrapconst_ops
    from . import muv_preserve_uv_aspect

import bpy


def view3d_uvmap_menu_fn(self, context):
    self.layout.separator()
    self.layout.menu(muv_menu.MUV_CPUVMenu.bl_idname, icon="IMAGE_COL")
    self.layout.operator(muv_fliprot_ops.MUV_FlipRot.bl_idname, icon="IMAGE_COL")
    self.layout.menu(muv_menu.MUV_TransUVMenu.bl_idname, icon="IMAGE_COL")
    self.layout.operator(muv_mvuv_ops.MUV_MVUV.bl_idname, icon="IMAGE_COL")
    self.layout.menu(muv_menu.MUV_TexLockMenu.bl_idname, icon="IMAGE_COL")
    self.layout.operator(
        muv_mirroruv_ops.MUV_MirrorUV.bl_idname, icon="IMAGE_COL")
    self.layout.menu(muv_menu.MUV_WSUVMenu.bl_idname, icon="IMAGE_COL")
    self.layout.operator(
        muv_unwrapconst_ops.MUV_UnwrapConstraint.bl_idname, icon='IMAGE_COL')
    self.layout.menu(
        muv_preserve_uv_aspect.MUV_PreserveUVAspectMenu.bl_idname,
        icon='IMAGE_COL')


def image_uvs_menu_fn(self, context):
    self.layout.separator()
    self.layout.operator(muv_packuv_ops.MUV_PackUV.bl_idname, icon="IMAGE_COL")


def view3d_object_menu_fn(self, context):
    self.layout.separator()
    self.layout.menu(muv_menu.MUV_CPUVObjMenu.bl_idname, icon="IMAGE_COL")


def register():
    bpy.utils.register_module(__name__)
    bpy.types.VIEW3D_MT_uv_map.append(view3d_uvmap_menu_fn)
    bpy.types.IMAGE_MT_uvs.append(image_uvs_menu_fn)
    bpy.types.VIEW3D_MT_object.append(view3d_object_menu_fn)
    try:
        bpy.types.VIEW3D_MT_Object.append(view3d_object_menu_fn)
    except:
        pass
    muv_props.init_props(bpy.types.Scene)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.VIEW3D_MT_uv_map.remove(view3d_uvmap_menu_fn)
    bpy.types.IMAGE_MT_uvs.remove(image_uvs_menu_fn)
    bpy.types.VIEW3D_MT_object.remove(view3d_object_menu_fn)
    try:
        bpy.types.VIEW3D_MT_Object.remove(view3d_object_menu_fn)
    except:
        pass
    muv_props.clear_props(bpy.types.Scene)


if __name__ == "__main__":
    register()
