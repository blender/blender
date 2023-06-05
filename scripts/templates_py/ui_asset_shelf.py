import bpy


class MyAssetShelf(bpy.types.AssetShelf):
    bl_space_type = 'VIEW_3D'
    bl_idname = "my_template.my_material_asset_shelf"

    @classmethod
    def poll(cls, context):
        return bool(context.object and context.object.mode == 'OBJECT')

    @classmethod
    def asset_poll__(cls, asset):
        return asset.file_data.id_type in {'MATERIAL', 'OBJECT'}


def register():
    bpy.utils.register_class(MyAssetShelf)


def unregister():
    bpy.utils.unregister_class(MyAssetShelf)


if __name__ == "__main__":
    register()
