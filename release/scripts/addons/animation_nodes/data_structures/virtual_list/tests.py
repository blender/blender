from mathutils import Vector
from unittest import TestCase
from . virtual_list import VirtualPyList
from . virtual_clists import VirtualLongList
from .. lists.base_lists import Vector3DList, LongList

vector = Vector((1, 2, 3))
pyVectorList = [Vector((1, 2, 3)), Vector((4, 5, 6)), Vector((7, 8, 9))]
copyVector = lambda x: x.copy()


class TestVirtualPyList(TestCase):
    def test_Element_NoCopy_Value(self):
        vList = VirtualPyList.fromElement(vector)
        self.assertEqual(vList[0], vector)
        self.assertEqual(vList[123], vector)
        self.assertEqual(vList[-12353], vector)

    def test_Element_NoCopy_Reference(self):
        vList = VirtualPyList.fromElement(vector)
        self.assertIs(vList[10], vector)
        self.assertIs(vList[20], vector)
        self.assertIs(vList[30], vector)

    def test_Element_Copy_Value(self):
        vList = VirtualPyList.fromElement(vector, copyVector)
        self.assertEqual(vList[10], vector)
        self.assertEqual(vList[20], vector)
        self.assertEqual(vList[-20], vector)

    def test_Element_Copy_Reference(self):
        vList = VirtualPyList.fromElement(vector, copyVector)
        self.assertIsNot(vList[10], vector)
        self.assertIsNot(vList[20], vector)
        self.assertIsNot(vList[10], vList[20])

    def test_List_NoCopy_Value(self):
        vList = VirtualPyList.fromList(pyVectorList, default = None)
        self.assertEqual(vList[0], pyVectorList[0])
        self.assertEqual(vList[2], pyVectorList[2])
        self.assertEqual(vList[3], pyVectorList[0])
        self.assertEqual(vList[4], pyVectorList[1])
        self.assertEqual(vList[123], pyVectorList[123 % len(pyVectorList)])

    def test_List_NoCopy_Reference(self):
        vList = VirtualPyList.fromList(pyVectorList, default = None)
        self.assertIs(vList[0], pyVectorList[0])
        self.assertIs(vList[2], pyVectorList[2])
        self.assertIs(vList[3], pyVectorList[0])
        self.assertIs(vList[4], pyVectorList[1])
        self.assertIs(vList[123], pyVectorList[123 % len(pyVectorList)])

    def test_List_NoCopy_Default_Value(self):
        vList = VirtualPyList.fromList([], default = vector)
        self.assertEqual(vList[0], vector)
        self.assertEqual(vList[10], vector)
        self.assertEqual(vList[-20], vector)

    def test_List_NoCopy_Default_Reference(self):
        vList = VirtualPyList.fromList([], default = vector)
        self.assertIs(vList[0], vector)
        self.assertIs(vList[10], vector)
        self.assertIs(vList[-20], vector)

    def test_List_Copy_Value(self):
        vList = VirtualPyList.fromList(pyVectorList, default = None, copy = copyVector)
        self.assertEqual(vList[0], pyVectorList[0])
        self.assertEqual(vList[2], pyVectorList[2])
        self.assertEqual(vList[3], pyVectorList[0])
        self.assertEqual(vList[4], pyVectorList[1])
        self.assertEqual(vList[123], pyVectorList[123 % len(pyVectorList)])

    def test_List_Copy_Reference(self):
        vList = VirtualPyList.fromList(pyVectorList, default = None, copy = copyVector)
        self.assertIsNot(vList[0], pyVectorList[0])
        self.assertIsNot(vList[2], pyVectorList[2])
        self.assertIsNot(vList[3], pyVectorList[0])
        self.assertIsNot(vList[4], pyVectorList[1])
        self.assertIsNot(vList[123], pyVectorList[123 % len(pyVectorList)])

    def test_List_Copy_Default_Value(self):
        vList = VirtualPyList.fromList([], default = vector, copy = copyVector)
        self.assertEqual(vList[0], vector)
        self.assertEqual(vList[10], vector)
        self.assertEqual(vList[-20], vector)

    def test_List_Copy_Default_Reference(self):
        vList = VirtualPyList.fromList([], default = vector, copy = copyVector)
        self.assertIsNot(vList[0], vector)
        self.assertIsNot(vList[10], vector)
        self.assertIsNot(vList[-20], vector)

    def test_List_NonPy(self):
        vecs = Vector3DList.fromValues([(1, 2, 3), (4, 5, 6)])
        vList = VirtualPyList.fromList(vecs, Vector((0, 0, 0)))
        self.assertEqual(vList[0], Vector((1, 2, 3)))
        self.assertEqual(vList[1], Vector((4, 5, 6)))
        self.assertEqual(vList[10], Vector((1, 2, 3)))
        self.assertEqual(vList[-1], Vector((4, 5, 6)))

    def test_List_Materialize(self):
        original = [1, 2, 3]
        vList = VirtualPyList.fromList(original, 0)

        newList = vList.materialize(5)
        self.assertEqual(len(newList), 5)
        self.assertEqual(newList[2], 3)
        self.assertEqual(newList[4], 2)

        newList = vList.materialize(3, canUseOriginal = False)
        self.assertIsNot(original, newList)
        self.assertEqual(original, newList)

        newList = vList.materialize(3, canUseOriginal = True)
        self.assertIs(original, newList)
        self.assertEqual(original, newList)

    def test_CList_Materialize(self):
        original = LongList.fromValues([1, 2, 3])
        vList = VirtualLongList.fromList(original, 0)

        newList = vList.materialize(5)
        self.assertEqual(len(newList), 5)
        self.assertEqual(newList[2], 3)
        self.assertEqual(newList[4], 2)

        newList = vList.materialize(3, canUseOriginal = False)
        self.assertIsNot(original, newList)
        self.assertEqual(original, newList)

        newList = vList.materialize(3, canUseOriginal = True)
        self.assertIs(original, newList)
        self.assertEqual(original, newList)