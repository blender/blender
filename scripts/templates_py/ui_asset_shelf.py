import bpy


class MyAssetShelf(bpy.types.AssetShelf):
    bl_space_type = 'VIEW_3D'
    bl_idname = "VIEW3D_AST_my_asset_shelf"

    # Tell the shelf which data-block types to show. It is highly recommended
    # to use these filters, as they avoid slowdowns when there are many assets
    # of irrelevant data-block types.
    # If no filter is set, all data-block types will show.
    filter_material = True
    filter_object = True

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT'


def register():
    bpy.utils.register_class(MyAssetShelf)


def unregister():
    bpy.utils.unregister_class(MyAssetShelf)


if __name__ == "__main__":
    register()
