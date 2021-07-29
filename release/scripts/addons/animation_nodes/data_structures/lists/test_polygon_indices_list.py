from unittest import TestCase
from . base_lists import LongList
from . polygon_indices_list import PolygonIndicesList

class TestAppend(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList()

    def testNormal(self):
        self.list.append((1, 2, 3))
        self.list.append((4, 5, 6, 7))
        self.list.append((7, 5, 3))
        self.assertEqual(self.list[0], (1, 2, 3))
        self.assertEqual(self.list[1], (4, 5, 6, 7))
        self.assertEqual(self.list[2], (7, 5, 3))

    def testTooShort(self):
        with self.assertRaises(TypeError):
            self.list.append((3, 5))

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.list.append((4, "abc", 3))

    def testNegativeNumbers(self):
        with self.assertRaises(TypeError):
            self.list.append((4, -3, 6))

class TestCopy(TestCase):
    def testNormal(self):
        self.list = PolygonIndicesList.fromValues([(1, 2, 3), (4, 5, 6, 7)])
        copy = self.list.copy()
        self.assertEqual(len(copy), 2)
        self.assertEqual(copy[0], (1, 2, 3))
        self.assertEqual(copy[1], (4, 5, 6, 7))

class TestReversed(TestCase):
    def testNormal(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (4, 5, 6, 7), (8, 9, 0)])
        newList = self.list.reversed()
        self.assertEqual(newList[0], (8, 9, 0))
        self.assertEqual(newList[1], (4, 5, 6, 7))
        self.assertEqual(newList[2], (1, 2, 3))

class TestJoin(TestCase):
    def testNormal(self):
        l1 = PolygonIndicesList.fromValues([(1, 2, 3), (4, 5, 6, 7)])
        l2 = PolygonIndicesList.fromValues([(2, 4, 7, 8), (1, 2, 3, 4)])
        l3 = PolygonIndicesList.fromValues([(6, 5, 4), (2, 3, 4, 5, 6)])
        newList = PolygonIndicesList.join(l1, l2, l3)
        self.assertEqual(len(newList), 6)
        self.assertEqual(newList[0], (1, 2, 3))
        self.assertEqual(newList[3], (1, 2, 3, 4))
        self.assertEqual(newList[5], (2, 3, 4, 5, 6))

class TestCopyWithNewOrder(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (4, 5, 6, 7), (8, 9, 0)])

    def testNormal(self):
        order = LongList.fromValues([1, 2, 0, 1])
        newList = self.list.copyWithNewOrder(order)
        self.assertEqual(len(newList), 4)
        self.assertEqual(newList[0], (4, 5, 6, 7))
        self.assertEqual(newList[1], (8, 9, 0))
        self.assertEqual(newList[2], (1, 2, 3))
        self.assertEqual(newList[3], (4, 5, 6, 7))

    def testEmptyList(self):
        myList = PolygonIndicesList()
        order = LongList.fromValues([1])
        with self.assertRaises(IndexError):
            newList = myList.copyWithNewOrder(order)

    def testTooHighIndex(self):
        order = LongList.fromValues([1, 0, 4, 2])
        with self.assertRaises(IndexError):
            newList = self.list.copyWithNewOrder(order)

class TestGetValuesInSlice(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (4, 5, 6, 7), (8, 9, 0), (3, 4, 5),
            (5, 4, 2), (2, 7, 6), (3, 4, 6, 2, 8), (5, 3, 2)])

    def testStart(self):
        newList = self.list[:3]
        self.assertEqual(len(newList), 3)
        self.assertEqual(newList[0], (1, 2, 3))
        self.assertEqual(newList[1], (4, 5, 6, 7))
        self.assertEqual(newList[2], (8, 9, 0))

    def testEnd(self):
        newList = self.list[-3:]
        self.assertEqual(len(newList), 3)
        self.assertEqual(newList[0], (2, 7, 6))
        self.assertEqual(newList[1], (3, 4, 6, 2, 8))
        self.assertEqual(newList[2], (5, 3, 2))

    def testComplex(self):
        newList = self.list[2:-1:2]
        self.assertEqual(len(newList), 3)
        self.assertEqual(newList[0], (8, 9, 0))
        self.assertEqual(newList[1], (5, 4, 2))
        self.assertEqual(newList[2], (3, 4, 6, 2, 8))

class TestGetValuesInIndexList(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (4, 5, 6, 7), (8, 9, 0), (3, 4, 5),
            (5, 4, 2), (2, 7, 6), (3, 4, 6, 2, 8), (5, 3, 2)])

    def testZeroElements(self):
        newList = self.list[[]]
        self.assertEqual(len(newList), 0)

    def testOneElement(self):
        newList = self.list[(1, )]
        self.assertEqual(len(newList), 1)
        self.assertEqual(newList[0], (4, 5, 6, 7))

    def testMoreElements(self):
        newList = self.list[(4, 6, 1)]
        self.assertEqual(len(newList), 3)
        self.assertEqual(newList[0], (5, 4, 2))
        self.assertEqual(newList[1], (3, 4, 6, 2, 8))
        self.assertEqual(newList[2], (4, 5, 6, 7))

    def testNegativeIndices(self):
        newList = self.list[(0, -1, -4)]
        self.assertEqual(len(newList), 3)
        self.assertEqual(newList[0], (1, 2, 3))
        self.assertEqual(newList[1], (5, 3, 2))
        self.assertEqual(newList[2], (5, 4, 2))

    def testTooHighIndex(self):
        with self.assertRaises(IndexError):
            newList = self.list[(1, 2, 3, 20, 5)]

    def testTooLowIndex(self):
        with self.assertRaises(IndexError):
            newList = self.list[(1, 2, 3, -5, -20)]

class TestAdd(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (4, 5, 6, 7), (8, 9, 0)])

    def testSameType(self):
        otherList = PolygonIndicesList.fromValues([(5, 6, 7), (1, 2, 3)])
        result = self.list + otherList
        self.assertEqual(len(result), 5)
        self.assertEqual(result[0], (1, 2, 3))
        self.assertEqual(result[2], (8, 9, 0))
        self.assertEqual(result[3], (5, 6, 7))
        self.assertEqual(result[4], (1, 2, 3))

    def testListLeft(self):
        otherList = [(5, 6, 7), (1, 2, 3)]
        result = otherList + self.list
        self.assertEqual(len(result), 5)
        self.assertEqual(result[0], (5, 6, 7))
        self.assertEqual(result[1], (1, 2, 3))
        self.assertEqual(result[2], (1, 2, 3))
        self.assertEqual(result[4], (8, 9, 0))

    def testListRight(self):
        otherList = [(5, 6, 7), (1, 2, 3)]
        result = self.list + otherList
        self.assertEqual(len(result), 5)
        self.assertEqual(result[0], (1, 2, 3))
        self.assertEqual(result[2], (8, 9, 0))
        self.assertEqual(result[3], (5, 6, 7))
        self.assertEqual(result[4], (1, 2, 3))

class TestMultiply(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (4, 5, 6, 7), (8, 9, 0)])

    def testListLeft(self):
        result = self.list * 3
        self.assertEqual(len(result), 9)
        self.assertEqual(result[0], (1, 2, 3))
        self.assertEqual(result[1], (4, 5, 6, 7))
        self.assertEqual(result[5], (8, 9, 0))
        self.assertEqual(result[6], (1, 2, 3))

    def testListRight(self):
        result = 3 * self.list
        self.assertEqual(len(result), 9)
        self.assertEqual(result[0], (1, 2, 3))
        self.assertEqual(result[1], (4, 5, 6, 7))
        self.assertEqual(result[5], (8, 9, 0))
        self.assertEqual(result[6], (1, 2, 3))

    def testNegativeFactor(self):
        result = -5 * self.list
        self.assertEqual(len(result), 0)

class TestIndex(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (4, 5, 6, 7), (8, 9, 0)])

    def testExists(self):
        result0 = self.list.index((1, 2, 3))
        self.assertEqual(result0, 0)
        result1 = self.list.index((4, 5, 6, 7))
        self.assertEqual(result1, 1)
        result2 = self.list.index((8, 9, 0))
        self.assertEqual(result2, 2)

    def testDoesNotExist(self):
        result0 = self.list.index((3, 4))
        self.assertEqual(result0, -1)
        result1 = self.list.index((2, 3, 4))
        self.assertEqual(result1, -1)
        result2 = self.list.index((4, 5, 6, 7, 8))
        self.assertEqual(result2, -1)

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.list.index("abc")

    def testNegativeNumbers(self):
        with self.assertRaises(Exception):
            self.list.index((3, -1, 4))

class TestCount(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (3, 4, 6, 2, 8), (5, 4, 2), (1, 2, 3),
            (5, 4, 2), (5, 4, 2), (3, 4, 6, 2, 8), (5, 3, 2)])

    def testExists(self):
        result0 = self.list.count((1, 2, 3))
        self.assertEqual(result0, 2)
        result1 = self.list.count((3, 4, 6, 2, 8))
        self.assertEqual(result1, 2)
        result2 = self.list.count((5, 4, 2))
        self.assertEqual(result2, 3)
        result3 = self.list.count((5, 3, 2))
        self.assertEqual(result3, 1)

    def testDoesNotExist(self):
        result0 = self.list.count((0, 1, 2))
        self.assertEqual(result0, 0)
        result1 = self.list.count((5, 4, 2, 1, 2, 3))
        self.assertEqual(result1, 0)

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.list.count("abc")

    def testNegativeNumbers(self):
        with self.assertRaises(Exception):
            self.list.count((3, -2, 5))

class TestSetElement(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (3, 4, 6, 2, 8), (5, 4, 2), (1, 2, 3)])

    def testSameLength(self):
        self.list[0] = (5, 6, 7)
        self.list[1] = (5, 6, 7, 8, 9)

        self.assertEqual(len(self.list), 4)
        self.assertEqual(self.list[0], (5, 6, 7))
        self.assertEqual(self.list[1], (5, 6, 7, 8, 9))
        self.assertEqual(self.list[2], (5, 4, 2))
        self.assertEqual(self.list[3], (1, 2, 3))

    def testDifferentLength(self):
        self.list[0] = (5, 6, 7, 8, 9)
        self.list[1] = (5, 6, 7)

        self.assertEqual(len(self.list), 4)
        self.assertEqual(self.list[0], (5, 6, 7, 8, 9))
        self.assertEqual(self.list[1], (5, 6, 7))
        self.assertEqual(self.list[2], (5, 4, 2))
        self.assertEqual(self.list[3], (1, 2, 3))

    def testNegativeIndex(self):
        self.list[-2] = (1, 2, 3, 4, 5)
        self.assertEqual(self.list[0], (1, 2, 3))
        self.assertEqual(self.list[1], (3, 4, 6, 2, 8))
        self.assertEqual(self.list[2], (1, 2, 3, 4, 5))
        self.assertEqual(self.list[3], (1, 2, 3))

    def testNegativeNumbers(self):
        with self.assertRaises(TypeError):
            self.list[1] = (4, -6, 2)

    def testTooShort(self):
        with self.assertRaises(TypeError):
            self.list[1] = (3, 2)

    def testWrongIndex(self):
        with self.assertRaises(IndexError):
            self.list[7] = (1, 2, 3)
        with self.assertRaises(IndexError):
            self.list[-7] = (1, 2, 3)

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.list[1] = "abc"

class TestRemove(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (3, 4, 6, 2, 8), (5, 4, 2), (1, 2, 3)])

    def testExists(self):
        self.list.remove((3, 4, 6, 2, 8))
        self.assertEqual(len(self.list), 3)
        self.list.remove((1, 2, 3))
        self.assertEqual(len(self.list), 2)
        self.list.remove((1, 2, 3))
        self.assertEqual(len(self.list), 1)
        self.assertEqual(self.list[0], (5, 4, 2))

    def testDoesNotExist(self):
        with self.assertRaises(ValueError):
            self.list.remove((1, 2, 3, 4))
        with self.assertRaises(ValueError):
            self.list.remove((3, 3, 4, 5))

    def testWrongType(self):
        with self.assertRaises(ValueError):
            self.list.remove("abc")

    def testNegativeNumbers(self):
        with self.assertRaises(ValueError):
            self.list.remove((4, -2, 3))

class TestDeleteIndex(TestCase):
    def setUp(self):
        self.list = PolygonIndicesList.fromValues([
            (1, 2, 3), (3, 4, 6, 2, 8), (5, 4, 2), (1, 2, 3)])

    def testNormal(self):
        del self.list[1]
        self.assertEqual(len(self.list), 3)
        self.assertEqual(self.list[0], (1, 2, 3))
        self.assertEqual(self.list[1], (5, 4, 2))
        self.assertEqual(self.list[2], (1, 2, 3))

    def testNegativeIndex(self):
        del self.list[-2]
        self.assertEqual(len(self.list), 3)
        self.assertEqual(self.list[0], (1, 2, 3))
        self.assertEqual(self.list[1], (3, 4, 6, 2, 8))
        self.assertEqual(self.list[2], (1, 2, 3))

    def testTooHighIndex(self):
        with self.assertRaises(IndexError):
            del self.list[10]

    def testTooLowIndex(self):
        with self.assertRaises(IndexError):
            del self.list[-10]
