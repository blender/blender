# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_render_layer.py -- --testdir="/data/lib/tests/"
import unittest


# ############################################################
# Layer Collection Crawler
# ############################################################

def listbase_iter(data, struct, listbase):
    element = data.get_pointer((struct, listbase, b'first'))
    while element is not None:
        yield element
        element = element.get_pointer(b'next')


def linkdata_iter(collection, data):
    element = collection.get_pointer((data, b'first'))
    while element is not None:
        yield element
        element = element.get_pointer(b'next')


def get_layer_collection(layer_collection):
    data = {}
    flag = layer_collection.get(b'flag')

    data['is_visible']    = (flag & (1 << 0)) != 0;
    data['is_selectable'] = (flag & (1 << 1)) != 0;
    data['is_folded']     = (flag & (1 << 2)) != 0;

    scene_collection = layer_collection.get_pointer(b'scene_collection')
    if scene_collection is None:
        name = 'Fail!'
    else:
        name = scene_collection.get(b'name')
    data['name'] = name

    objects = []
    for link in linkdata_iter(layer_collection, b'object_bases'):
        ob_base = link.get_pointer(b'data')
        ob = ob_base.get_pointer(b'object')
        objects.append(ob.get((b'id', b'name'))[2:])
    data['objects'] = objects

    collections = {}
    for nested_layer_collection in linkdata_iter(layer_collection, b'layer_collections'):
        subname, subdata = get_layer_collection(nested_layer_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_layer(layer):
    data = {}
    name = layer.get(b'name')

    data['name'] = name
    data['active_object'] = layer.get((b'basact', b'object', b'id', b'name'))[2:]
    data['engine'] = layer.get(b'engine')

    objects = []
    for link in linkdata_iter(layer, b'object_bases'):
        ob = link.get_pointer(b'object')
        objects.append(ob.get((b'id', b'name'))[2:])
    data['objects'] = objects

    collections = {}
    for layer_collection in linkdata_iter(layer, b'layer_collections'):
        subname, subdata = get_layer_collection(layer_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_layers(scene):
    """Return all the render layers and their data"""
    layers = {}
    for layer in linkdata_iter(scene, b'render_layers'):
        name, data = get_layer(layer)
        layers[name] = data
    return layers


def get_scene_collection_objects(collection, listbase):
    objects = []
    for link in linkdata_iter(collection, listbase):
        ob = link.get_pointer(b'data')
        if ob is None:
            name  = 'Fail!'
        else:
            name = ob.get((b'id', b'name'))[2:]
        objects.append(name)
    return objects


def get_scene_collection(collection):
    """"""
    data = {}
    name = collection.get(b'name')

    data['name'] = name
    data['filter'] = collection.get(b'filter')

    data['objects'] = get_scene_collection_objects(collection, b'objects')
    data['filter_objects'] = get_scene_collection_objects(collection, b'filter_objects')

    collections = {}
    for nested_collection in linkdata_iter(collection, b'scene_collections'):
        subname, subdata = get_scene_collection(nested_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_scene_collections(scene):
    """Return all the scene collections ahd their data"""
    master_collection = scene.get_pointer(b'collection')
    return get_scene_collection(master_collection)


def query_scene(filepath, name, callbacks):
    """Return the equivalent to bpy.context.scene"""
    import blendfile
    with blendfile.open_blend(filepath) as blend:
        scenes = [block for block in blend.blocks if block.code == b'SC']
        for scene in scenes:
            if scene.get((b'id', b'name'))[2:] == name:
                output = []
                for callback in callbacks:
                    output.append(callback(scene))
                return output


# ############################################################
# Utils
# ############################################################

def import_blendfile():
    import bpy
    import os, sys
    path = os.path.join(
            bpy.utils.resource_path('LOCAL'),
            'scripts',
            'addons',
            'io_blend_utils',
            'blend',
            )

    if path not in sys.path:
        sys.path.append(path)


def dump(data):
    import json
    return json.dumps(
            data,
            sort_keys=True,
            indent=4,
            separators=(',', ': '),
            )


# ############################################################
# Tests
# ############################################################

PDB = False
DUMP_DIFF = True

def compare_files(file_a, file_b):
    import filecmp

    if not filecmp.cmp(
        file_a,
        file_b):

        if DUMP_DIFF:
            import subprocess
            subprocess.call(["diff", "-u", file_a, file_b])

        if PDB:
            import pdb
            print("Files differ:", file_a, file_b)
            pdb.set_trace()

        return False

    return True


class UnitsTesting(unittest.TestCase):
    _test_simple = False

    @classmethod
    def setUpClass(cls):
        """Runs once"""
        cls.pretest_import_blendfile()
        cls.pretest_parsing()

    @classmethod
    def setUp(cls):
        """Runs once per test"""
        import bpy
        bpy.ops.wm.read_factory_settings()

    def path_exists(self, filepath):
        import os
        self.assertTrue(
                os.path.exists(filepath),
                "Test file \"{0}\" not found".format(filepath))

    @classmethod
    def get_root(cls):
        """
        return the folder with the test files
        """
        arguments = {}
        for argument in extra_arguments:
            name, value = argument.split('=')
            cls.assertTrue(name and name.startswith("--"), "Invalid argument \"{0}\"".format(argument))
            cls.assertTrue(value, "Invalid argument \"{0}\"".format(argument))
            arguments[name[2:]] = value.strip('"')

        return arguments.get('testdir')

    @classmethod
    def pretest_parsing(cls):
        """
        Test if the arguments are properly set, and store ROOT
        name has extra _ because we need this test to run first
        """
        root = cls.get_root()
        cls.assertTrue(root, "Testdir not set")

    @staticmethod
    def pretest_import_blendfile():
        """
        Make sure blendfile imports with no problems
        name has extra _ because we need this test to run first
        """
        import_blendfile()
        import blendfile

    def do_scene_write_read(self, filepath_layers, filepath_layers_json, data_callbacks, do_read):
        """
        See if write/read is working for scene collections and layers
        """
        import bpy
        import os
        import tempfile
        import filecmp

        with tempfile.TemporaryDirectory() as dirpath:
            (self.path_exists(f) for f in (filepath_layers, filepath_layers_json))

            filepath_doversion = os.path.join(dirpath, 'doversion.blend')
            filepath_saved = os.path.join(dirpath, 'doversion_saved.blend')
            filepath_read_json = os.path.join(dirpath, "read.json")

            # doversion + write test
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_doversion)

            datas = query_scene(filepath_doversion, 'Main', data_callbacks)
            self.assertTrue(datas, "Data is not valid")

            filepath_doversion_json = os.path.join(dirpath, "doversion.json")
            with open(filepath_doversion_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_doversion_json,
                filepath_layers_json,
                ),
                "Run: test_scene_write_layers")

            if do_read:
                # read test, simply open and save the file
                bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_doversion)
                bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

                datas = query_scene(filepath_saved, 'Main', data_callbacks)
                self.assertTrue(datas, "Data is not valid")

                with open(filepath_read_json, "w") as f:
                    for data in datas:
                        f.write(dump(data))

                self.assertTrue(compare_files(
                    filepath_read_json,
                    filepath_layers_json,
                    ),
                    "Scene dump files differ")

    def test_scene_write_collections(self):
        """
        See if the doversion and writing are working for scene collections
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers_simple.json')

        self.do_scene_write_read(
                filepath_layers,
                filepath_layers_json,
                (get_scene_collections,),
                False)

    def test_scene_write_layers(self):
        """
        See if the doversion and writing are working for collections and layers
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers.json')

        self.do_scene_write_read(
                filepath_layers,
                filepath_layers_json,
                (get_scene_collections, get_layers),
                False)

    def test_scene_read_collections(self):
        """
        See if read is working for scene collections
        (run `test_scene_write_colections` first)
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers_simple.json')

        self.do_scene_write_read(
                filepath_layers,
                filepath_layers_json,
                (get_scene_collections,),
                True)

    def test_scene_read_layers(self):
        """
        See if read is working for scene layers
        (run `test_scene_write_layers` first)
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers.json')

        self.do_scene_write_read(
                filepath_layers,
                filepath_layers_json,
                (get_scene_collections, get_layers),
                True)

    def do_scene_copy(self, filepath_json_reference, copy_mode, data_callbacks):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            (self.path_exists(f) for f in (
                filepath_layers,
                filepath_json_reference,
                ))

            filepath_saved = os.path.join(dirpath, '{0}.blend'.format(copy_mode))
            filepath_json = os.path.join(dirpath, "{0}.json".format(copy_mode))

            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            bpy.ops.scene.new(type=copy_mode)
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

            datas = query_scene(filepath_saved, 'Main.001', data_callbacks)
            self.assertTrue(datas, "Data is not valid")

            with open(filepath_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_json,
                filepath_json_reference,
                ),
                "Scene copy \"{0}\" test failed".format(copy_mode.title()))

    def test_scene_collections_copy_full(self):
        """
        See if scene copying 'FULL_COPY' is working for scene collections
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_full_simple.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'FULL_COPY',
                (get_scene_collections,))

    def test_scene_collections_link(self):
        """
        See if scene copying 'LINK_OBJECTS' is working for scene collections
        """
        import os
        ROOT = self.get_root()

        # note: nothing should change, so using `layers_simple.json`
        filepath_layers_json_copy = os.path.join(ROOT, 'layers_simple.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'LINK_OBJECTS',
                (get_scene_collections,))

    def test_scene_layers_copy(self):
        """
        See if scene copying 'FULL_COPY' is working for scene layers
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_full.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'FULL_COPY',
                (get_scene_collections, get_layers))

    def test_scene_layers_link(self):
        """
        See if scene copying 'FULL_COPY' is working for scene layers
        """
        import os
        ROOT = self.get_root()

        filepath_layers_json_copy = os.path.join(ROOT, 'layers_copy_link.json')
        self.do_scene_copy(
                filepath_layers_json_copy,
                'LINK_OBJECTS',
                (get_scene_collections, get_layers))

    def do_syncing(self, filepath_json, unlink_mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')
            three_d = bpy.data.objects.get('T.3d')

            scene = bpy.context.scene

            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = scene.master_collection.collections['1'].collections.new('scorpion')

            # test linking sync
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)

            # test unlinking sync
            if unlink_mode in {'OBJECT', 'COLLECTION'}:
                scorpion.objects.link(three_d)
                scorpion.objects.unlink(three_d)

            if unlink_mode == 'COLLECTION':
                scorpion.objects.link(three_d)
                scene.master_collection.collections['1'].collections.remove(subzero)
                scene.master_collection.collections['1'].collections.remove(scorpion)

            # save file
            filepath_nested = os.path.join(dirpath, 'nested.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_nested)

            # get the generated json
            datas = query_scene(filepath_nested, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_nested_json = os.path.join(dirpath, "nested.json")
            with open(filepath_nested_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_nested_json,
                filepath_json,
                ),
                "Scene dump files differ")

    def test_syncing_link(self):
        """
        See if scene collections and layer collections are in sync
        when we create new subcollections and link new objects
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_nested.json')
        self.do_syncing(filepath_json, 'NONE')

    def test_syncing_unlink_object(self):
        """
        See if scene collections and layer collections are in sync
        when we create new subcollections, link new objects and unlink
        some.
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_nested.json')
        self.do_syncing(filepath_json, 'OBJECT')

    def test_syncing_unlink_collection(self):
        """
        See if scene collections and layer collections are in sync
        when we create new subcollections, link new objects and unlink full collections
        some.
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers.json')
        self.do_syncing(filepath_json, 'COLLECTION')

    def do_layer_linking(self, filepath_json, link_mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')

            scene = bpy.context.scene

            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')

            # test linking sync
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)

            # test unlinking sync
            layer = scene.render_layers.new('Fresh new Layer')

            if link_mode in {'COLLECTION_LINK', 'COLLECTION_UNLINK'}:
                layer.collections.link(subzero)

            if link_mode == 'COLLECTION_UNLINK':
                initial_collection = layer.collections['Master Collection']
                layer.collections.unlink(initial_collection)

            # save file
            filepath_nested = os.path.join(dirpath, 'nested.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_nested)

            # get the generated json
            datas = query_scene(filepath_nested, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_nested_json = os.path.join(dirpath, "nested.json")
            with open(filepath_nested_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_nested_json,
                filepath_json,
                ),
                "Scene dump files differ")

    def test_syncing_layer_new(self):
        """
        See if the creation of new layers is going well
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_new_layer.json')
        self.do_layer_linking(filepath_json, 'LAYER_NEW')

    def test_syncing_layer_collection_link(self):
        """
        See if the creation of new layers is going well
        And linking a new scene collection in the layer works
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_layer_collection_link.json')
        self.do_layer_linking(filepath_json, 'COLLECTION_LINK')

    def test_syncing_layer_collection_unlink(self):
        """
        See if the creation of new layers is going well
        And unlinking the origin scene collection works
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_layer_collection_unlink.json')
        self.do_layer_linking(filepath_json, 'COLLECTION_UNLINK')

    def test_active_collection(self):
        """
        See if active collection index is working
        layer.collections.active_index works recursively
        """
        import bpy
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')

        # open file
        bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

        # create sub-collections
        three_b = bpy.data.objects.get('T.3b')
        three_c = bpy.data.objects.get('T.3c')

        scene = bpy.context.scene
        subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
        scorpion = subzero.collections.new('scorpion')
        subzero.objects.link(three_b)
        scorpion.objects.link(three_c)
        layer = scene.render_layers.new('Fresh new Layer')
        layer.collections.link(subzero)

        lookup = [
                'Master Collection',
                '1',
                'sub-zero',
                'scorpion',
                '2',
                '3',
                '4',
                '5',
                'sub-zero',
                'scorpion']

        for i, name in enumerate(lookup):
            layer.collections.active_index = i
            self.assertEqual(name, layer.collections.active.name,
                    "Collection index mismatch: [{0}] : {1} != {2}".format(
                        i, name, layer.collections.active.name))

    def do_object_delete(self, del_mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')
            filepath_reference_json = os.path.join(ROOT, 'layers_object_delete.json')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_d = bpy.data.objects.get('T.3d')

            scene = bpy.context.scene

            # mangle the file a bit with some objects linked across collections
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_d)
            scorpion.objects.link(three_b)
            scorpion.objects.link(three_d)

            # object to delete
            ob = three_d

            # delete object
            if del_mode == 'DATA':
                bpy.data.objects.remove(ob, do_unlink=True)

            elif del_mode == 'OPERATOR':
                bpy.ops.object.select_all(action='DESELECT')
                ob.select_set(action='SELECT')
                bpy.ops.object.delete()

            # save file
            filepath_generated = os.path.join(dirpath, 'generated.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_generated)

            # get the generated json
            datas = query_scene(filepath_generated, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_generated_json = os.path.join(dirpath, "generated.json")
            with open(filepath_generated_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_generated_json,
                filepath_reference_json,
                ),
                "Scene dump files differ")

    def test_object_delete_data(self):
        """
        See if objects are removed correctly from all related collections
        bpy.data.objects.remove()
        """
        self.do_object_delete('DATA')

    def test_object_delete_operator(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.object.del()
        """
        self.do_object_delete('OPERATOR')

    def do_link(self, master_collection):
        import bpy
        self.assertEqual(master_collection.name, "Master Collection")
        self.assertEqual(master_collection, bpy.context.scene.master_collection)
        master_collection.objects.link(bpy.data.objects.new('object', None))

    def test_link_scene(self):
        """
        See if we can link objects
        """
        import bpy
        master_collection = bpy.context.scene.master_collection
        self.do_link(master_collection)

    def test_link_context(self):
        """
        See if we can link objects via bpy.context.scene_collection
        """
        import bpy
        bpy.context.scene.render_layers.active_index = len(bpy.context.scene.render_layers) - 1
        master_collection = bpy.context.scene_collection
        self.do_link(master_collection)

    def test_operator_context(self):
        """
        See if render layer context is properly set/get with operators overrides
        when we set render_layer in context, the collection should change as well
        """
        import bpy
        import os

        class SampleOperator(bpy.types.Operator):
            bl_idname = "testing.sample"
            bl_label = "Sample Operator"

            render_layer = bpy.props.StringProperty(
                    default="Not Set",
                    options={'SKIP_SAVE'},
                    )

            scene_collection = bpy.props.StringProperty(
                    default="",
                    options={'SKIP_SAVE'},
                    )

            use_verbose = bpy.props.BoolProperty(
                    default=False,
                    options={'SKIP_SAVE'},
                    )

            def execute(self, context):
                render_layer = context.render_layer
                ret = {'FINISHED'}

                # this is simply playing safe
                if render_layer.name != self.render_layer:
                    if self.use_verbose:
                        print('ERROR: Render Layer mismatch: "{0}" != "{1}"'.format(
                            render_layer.name, self.render_layer))
                    ret = {'CANCELLED'}

                scene_collection_name = None
                if self.scene_collection:
                    scene_collection_name = self.scene_collection
                else:
                    scene_collection_name = render_layer.collections.active.name

                # while this is the real test
                if context.scene_collection.name != scene_collection_name:
                    if self.use_verbose:
                        print('ERROR: Scene Collection mismatch: "{0}" != "{1}"'.format(
                            context.scene_collection.name, scene_collection_name))
                    ret = {'CANCELLED'}
                return ret

        bpy.utils.register_class(SampleOperator)

        # open sample file
        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

        # change the file
        three_b = bpy.data.objects.get('T.3b')
        three_c = bpy.data.objects.get('T.3c')
        scene = bpy.context.scene
        subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
        scorpion = subzero.collections.new('scorpion')
        subzero.objects.link(three_b)
        scorpion.objects.link(three_c)
        layer = scene.render_layers.new('Fresh new Layer')
        layer.collections.unlink(layer.collections.active)
        layer.collections.link(subzero)
        layer.collections.active_index = 3
        self.assertEqual(layer.collections.active.name, 'scorpion')

        scene = bpy.context.scene
        scene.render_layers.active_index = len(scene.render_layers) - 2
        self.assertEqual(scene.render_layers.active.name, "Render Layer")

        # old layer
        self.assertEqual(bpy.ops.testing.sample(render_layer='Render Layer', use_verbose=True), {'FINISHED'})

        # expected to fail
        self.assertTrue(bpy.ops.testing.sample(render_layer=layer.name), {'CANCELLED'})

        # set render layer and scene collection
        override = bpy.context.copy()
        override["render_layer"] = layer
        override["scene_collection"] = subzero
        self.assertEqual(bpy.ops.testing.sample(override,
            render_layer=layer.name,
            scene_collection=subzero.name, # 'sub-zero'
            use_verbose=True), {'FINISHED'})

        # set only render layer
        override = bpy.context.copy()
        override["render_layer"] = layer

        self.assertEqual(bpy.ops.testing.sample(override,
            render_layer=layer.name,
            scene_collection=layer.collections.active.name, # 'scorpion'
            use_verbose=True), {'FINISHED'})

    def do_object_add(self, filepath_json, add_mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')

            scene = bpy.context.scene
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)
            layer = scene.render_layers.new('Fresh new Layer')
            layer.collections.link(subzero)

            # change active collection
            layer.collections.active_index = 3
            self.assertEqual(layer.collections.active.name, 'scorpion', "Run: test_syncing_object_add")

            # change active layer
            override = bpy.context.copy()
            override["render_layer"] = layer
            override["scene_collection"] = layer.collections.active.collection

            # add new objects
            if add_mode == 'EMPTY':
                bpy.ops.object.add(override) # 'Empty'

            elif add_mode == 'CYLINDER':
                bpy.ops.mesh.primitive_cylinder_add(override) # 'Cylinder'

            elif add_mode == 'TORUS':
                bpy.ops.mesh.primitive_torus_add(override) # 'Torus'

            # save file
            filepath_objects = os.path.join(dirpath, 'objects.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_objects)

            # get the generated json
            datas = query_scene(filepath_objects, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_objects_json = os.path.join(dirpath, "objects.json")
            with open(filepath_objects_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_objects_json,
                filepath_json,
                ),
                "Scene dump files differ")

    def test_syncing_object_add_empty(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.object.add()
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_object_add_empty.json')
        self.do_object_add(filepath_json, 'EMPTY')

    def test_syncing_object_add_cylinder(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.mesh.primitive_cylinder_add()
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_object_add_cylinder.json')
        self.do_object_add(filepath_json, 'CYLINDER')

    def test_syncing_object_add_torus(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.mesh.primitive_torus_add()
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_object_add_torus.json')
        self.do_object_add(filepath_json, 'TORUS')

    def do_copy_object(self, mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')
            filepath_json = os.path.join(ROOT, 'layers_object_copy_duplicate.json')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')

            scene = bpy.context.scene
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)
            layer = scene.render_layers.new('Fresh new Layer')
            layer.collections.link(subzero)

            scene.render_layers.active_index = len(scene.render_layers) - 1

            if mode == 'DUPLICATE':
                # assuming the latest layer is the active layer
                bpy.ops.object.select_all(action='DESELECT')
                three_c.select_set(action='SELECT')
                bpy.ops.object.duplicate()

            elif mode == 'NAMED':
                bpy.ops.object.add_named(name=three_c.name)

            # save file
            filepath_objects = os.path.join(dirpath, 'objects.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_objects)

            # get the generated json
            datas = query_scene(filepath_objects, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_objects_json = os.path.join(dirpath, "objects.json")
            with open(filepath_objects_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_objects_json,
                filepath_json,
                ),
                "Scene dump files differ")

    def test_copy_object(self):
        """
        OBJECT_OT_duplicate
        """
        self.do_copy_object('DUPLICATE')

    def test_copy_object_named(self):
        """
        OBJECT_OT_add_named
        """
        self.do_copy_object('NAMED')


# ############################################################
# Main
# ############################################################

if __name__ == '__main__':
    import sys

    global extra_arguments
    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])
    unittest.main()
