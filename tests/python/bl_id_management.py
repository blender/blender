# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background -noaudio --python tests/python/bl_id_management.py -- --verbose
import bpy
import unittest
import random


class TestRNAIDTypes(unittest.TestCase):
    data_container_id = 'meshes'
    default_name = "Mesh"

    def test_rna_idtypes(self):
        # This test the coherence between ID types exposed in `bpy.data`, and these listed in `rna_enum_id_type_items`.
        new_args_extra = {
            "curves": {'type': 'CURVE'},
            "lightprobes": {'type': 'SPHERE'},
            "node_groups": {'type': 'CompositorNodeTree'},
            "textures": {'type': 'NONE'},
        }
        for container_id in dir(bpy.data):
            if container_id.startswith("rna") or container_id.startswith("__") or container_id.startswith("version"):
                continue
            container = getattr(bpy.data, container_id)
            if callable(container):
                continue
            if not hasattr(container, "__len__"):
                continue
            if len(container) == 0:
                if not hasattr(container, "new"):
                    continue
                if container_id in new_args_extra:
                    container.new("TestData", **new_args_extra[container_id])
                else:
                    container.new("TestData")
            self.assertIsNot(container[0].id_type, "")


class TestHelper:

    @property
    def data_container(self):
        return getattr(bpy.data, self.data_container_id)

    def clear_container(self):
        bpy.data.batch_remove(self.data_container)

    def add_to_container(self, name=""):
        return self.data_container.new(name)

    def remove_from_container(self, data=None, name=None, index=None):
        data_container = self.data_container
        if not data:
            if name:
                data = data_container[name]
            elif index:
                data = data_container[index]
        self.assertTrue(data is not None)
        data_container.remove(data)

    def add_items_with_randomized_names(self, number_items=1, name_prefix=None, name_suffix=""):
        if name_prefix is None:
            name_prefix = self.default_name
        for i in range(number_items):
            self.add_to_container(name=name_prefix + str(random.random())[2:5] + name_suffix)

    def ensure_proper_order(self):
        id_prev = None
        for id in self.data_container:
            if id_prev:
                self.assertTrue(id_prev.name < id.name or id.library)
            id_prev = id


class TestIdAddNameManagement(TestHelper, unittest.TestCase):
    data_container_id = 'meshes'
    default_name = "Mesh"

    def test_add_remove_single(self):
        self.clear_container()
        self.assertEqual(len(self.data_container), 0)
        data = self.add_to_container()
        self.assertEqual(len(self.data_container), 1)
        self.ensure_proper_order()
        self.remove_from_container(data=data)
        self.assertEqual(len(self.data_container), 0)

    def test_add_head_tail(self):
        self.clear_container()
        self.add_items_with_randomized_names(10)
        self.assertEqual(len(self.data_container), 10)
        self.ensure_proper_order()

        name_head = "AAA" + self.default_name
        data = self.add_to_container(name=name_head)
        self.assertEqual(len(self.data_container), 11)
        self.assertEqual(self.data_container[0].name, name_head)
        self.ensure_proper_order()

        name_tail = "ZZZ" + self.default_name
        data = self.add_to_container(name=name_tail)
        self.assertEqual(len(self.data_container), 12)
        self.assertEqual(self.data_container[-1].name, name_tail)
        self.ensure_proper_order()

    def test_add_long_names(self):
        self.clear_container()
        self.add_items_with_randomized_names(10)
        self.assertEqual(len(self.data_container), 10)
        self.ensure_proper_order()

        for i in range(12000):
            self.add_to_container(name="ABCDEFGHIJKLMNOPQRSTUVWXYZ" * 3)
        self.assertEqual(len(self.data_container), 12010)
        self.ensure_proper_order()

        self.clear_container()
        self.add_items_with_randomized_names(100, name_prefix="", name_suffix="ABCDEFGHIJKLMNOPQRSTUVWXYZ" * 3)
        self.assertEqual(len(self.data_container), 100)
        self.ensure_proper_order()

    def test_add_invalid_number_suffixes(self):
        self.clear_container()

        name = "%s.%.3d" % (self.default_name, 1000000000)
        data = self.add_to_container(name=name)
        self.assertEqual(data.name, name)

        data = self.add_to_container(name=name)
        self.assertEqual(data.name, self.default_name + ".001")

        data = self.add_to_container(name=name)
        self.assertEqual(data.name, self.default_name + ".002")

        data = self.add_to_container(name=self.default_name)
        self.assertEqual(data.name, self.default_name)

        data = self.add_to_container(name="%s.%.3d" % (self.default_name, 0))
        self.assertEqual(data.name, self.default_name + ".000")

        data = self.add_to_container(name="%s.%.3d" % (self.default_name, 0))
        self.assertEqual(data.name, self.default_name + ".003")

        self.assertEqual(len(self.data_container), 6)
        self.ensure_proper_order()

    def test_add_use_smallest_free_number(self):
        self.clear_container()

        data = self.add_to_container(name="%s.%.3d" % (self.default_name, 2))
        self.assertEqual(data.name, self.default_name + ".002")

        data = self.add_to_container(name="%s.%.3d" % (self.default_name, 2))
        self.assertEqual(data.name, self.default_name + ".001")

        data = self.add_to_container(name="%s.%.3d" % (self.default_name, 2))
        self.assertEqual(data.name, self.default_name + ".003")

        for i in range(5, 1111):
            name = "%s.%.3d" % (self.default_name, i)
            data = self.add_to_container(name=name)
            self.assertEqual(data.name, name)

        for i in range(1112, 1200):
            name = "%s.%.3d" % (self.default_name, i)
            data = self.add_to_container(name=name)
            self.assertEqual(data.name, name)

        # Only slot available below 1024: 004.
        data = self.add_to_container(name="%s.%.3d" % (self.default_name, 2))
        self.assertEqual(data.name, self.default_name + ".004")

        # Slot available at 1111 is not 'found' and we get first highest free number, 1200.
        data = self.add_to_container(name="%s.%.3d" % (self.default_name, 2))
        self.assertEqual(data.name, self.default_name + ".1200")

        self.assertEqual(len(self.data_container), 1199)
        self.ensure_proper_order()


class TestIdRename(TestHelper, unittest.TestCase):
    data_container_id = 'meshes'
    default_name = "Mesh"

    def test_rename(self):
        self.clear_container()
        self.add_items_with_randomized_names(100)
        self.ensure_proper_order()

        data = self.data_container[0]
        data.name = "ZZZ" + data.name
        self.assertEqual(self.data_container[-1], data)
        self.ensure_proper_order()
        data.name = "AAA" + data.name
        self.assertEqual(self.data_container[0], data)
        self.ensure_proper_order()

        name = "%s.%.3d" % (self.default_name, 1000000000)
        data.name = name
        self.assertEqual(data.name, name)
        for dt in self.data_container:
            if dt is not data:
                data = dt
                break
        data.name = name
        # This can fail currently, see #71244.
        # ~ self.assertEqual(data.name, self.default_name + ".001")
        self.ensure_proper_order()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
