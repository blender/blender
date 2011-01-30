import bpy
from keyingsets_utils import *


class BUILTIN_KSI_hello(bpy.types.KeyingSetInfo):
    bl_label = "Hello World KeyingSet"

    # poll - test for whether Keying Set can be used at all
    def poll(ksi, context):
        return (context.active_object) or (context.selected_objects)

    # iterator - go over all relevant data, calling generate()
    def iterator(ksi, context, ks):
        for ob in context.selected_objects:
            ksi.generate(context, ks, ob)

    # generator - populate Keying Set with property paths to use
    def generate(ksi, context, ks, data):
        id_block = data.id_data

        ks.paths.add(id_block, "location")

        for i in range(5):
            ks.paths.add(id_block, "layers", i, group_method='NAMED', group_name="5x Hello Layers")

        ks.paths.add(id_block, "show_x_ray", group_method='NONE')

# manually register
bpy.types.register(BUILTIN_KSI_hello)
