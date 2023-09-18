import bpy


class MyAssetShelf(bpy.types.AssetShelf):
    bl_space_type = 'VIEW_3D'
    bl_idname = "VIEW3D_AST_my_asset_shelf"

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT'

    @classmethod
    def asset_poll(cls, asset):
        return asset.file_data.id_type in {'MATERIAL', 'OBJECT'}


def register():
    bpy.utils.register_class(MyAssetShelf)


def unregister():
    bpy.utils.unregister_class(MyAssetShelf)


if __name__ == "__main__":
    register()
