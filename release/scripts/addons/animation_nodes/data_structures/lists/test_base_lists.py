from mathutils import Vector
from unittest import TestCase
from . base_lists import IntegerList, FloatList

class TestInsertion(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 2, 3))

    def testAtStart(self):
        self.list.insert(0, 10)
        self.assertEqual(self.list, (10, 0, 1, 2, 3))

    def testAtEnd(self):
        self.list.insert(4, 10)
        self.assertEqual(self.list, (0, 1, 2, 3, 10))

    def testAfterEnd(self):
        self.list.insert(50, 10)
        self.assertEqual(self.list, (0, 1, 2, 3, 10))

    def testInMiddle(self):
        self.list.insert(2, 10)
        self.assertEqual(self.list, (0, 1, 10, 2, 3))

    def testLengthUpdate(self):
        self.list.insert(2, 1)
        self.assertEqual(len(self.list), 5)

    def testNegativeIndex(self):
        self.list.insert(-1, 10)
        self.assertEqual(self.list, (0, 1, 2, 10, 3))
        self.list.insert(-3, 20)
        self.assertEqual(self.list, (0, 1, 20, 2, 10, 3))
        self.list.insert(-100, 30)
        self.assertEqual(self.list, (30, 0, 1, 20, 2, 10, 3))

class TestRichComparison(TestCase):
    def testEquality_Left(self):
        a = IntegerList.fromValues((0, 1, 2, 3))

        self.assertTrue(a == (0, 1, 2, 3))
        self.assertTrue(a == [0, 1, 2, 3])
        self.assertFalse(a == [0, 1, 2, 3, 4])
        self.assertFalse(a == (0, 1, 2, 3, 4))

        self.assertFalse(a != (0, 1, 2, 3))
        self.assertFalse(a != [0, 1, 2, 3])
        self.assertTrue(a != [0, 1, 2, 3, 4])
        self.assertTrue(a != (0, 1, 2, 3, 4))

    def testEquality_Right(self):
        a = IntegerList.fromValues((0, 1, 2, 3))

        self.assertTrue((0, 1, 2, 3) == a)
        self.assertTrue([0, 1, 2, 3] == a)
        self.assertFalse([0, 1, 2, 3, 4] == a)
        self.assertFalse((0, 1, 2, 3, 4) == a)

        self.assertFalse((0, 1, 2, 3) != a)
        self.assertFalse([0, 1, 2, 3] != a)
        self.assertTrue([0, 1, 2, 3, 4] != a)
        self.assertTrue((0, 1, 2, 3, 4) != a)

    def testEquality_Both(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        b = IntegerList.fromValues((0, 1, 2, 3))
        c = IntegerList.fromValues((0, 1, 2, 3, 4))
        d = IntegerList.fromValues((0, 1, 2, 4))

        self.assertTrue(a == b)
        self.assertFalse(a == c)
        self.assertFalse(a == d)

        self.assertFalse(a != b)
        self.assertTrue(a != c)
        self.assertTrue(a != d)

class TestClear(TestCase):
    def testLengthAfterClear(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        self.assertEqual(len(a), 4)
        a.clear()
        self.assertEqual(len(a), 0)
        self.assertEqual(a, [])

class TestCopy(TestCase):
    def testDifferentMemoryAdresses(self):
        a = IntegerList()
        b = a.copy()
        self.assertNotEqual(id(a), id(b))

    def testIndependency(self):
        a = IntegerList()
        b = a.copy()
        a.append(5)
        self.assertEqual(len(a), 1)
        self.assertEqual(len(b), 0)

class TestAppend(TestCase):
    def testEmptyList(self):
        a = IntegerList()
        a.append(5)
        self.assertEqual(a, [5])

    def testLength(self):
        a = IntegerList.fromValues((1, 2, 3, 4))
        a.append(5)
        self.assertEqual(len(a), 5)

    def testNormal(self):
        a = IntegerList.fromValues((1, 2, 3))
        a.append(4)
        self.assertEqual(a, [1, 2, 3, 4])

    def testMany(self):
        a = IntegerList()
        for i in range(10):
            a.append(i)
        self.assertEqual(a, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9])

class TestExtend(TestCase):
    def testEmptyList(self):
        a = IntegerList()
        a.extend((1, 2, 3))
        self.assertEqual(a, [1, 2, 3])

    def testListInput(self):
        a = IntegerList.fromValues((1, 2, 3))
        a.extend([4, 5, 6])
        self.assertEqual(a, [1, 2, 3, 4, 5, 6])

    def testTupleInput(self):
        a = IntegerList.fromValues((1, 2, 3))
        a.extend((4, 5, 6))
        self.assertEqual(a, [1, 2, 3, 4, 5, 6])

    def testIteratorInput(self):
        a = IntegerList()
        a.extend(range(5))
        self.assertEqual(a, [0, 1, 2, 3, 4])

    def testGeneratorInput(self):
        a = IntegerList()
        gen = (i**2 for i in range(5))
        a.extend(gen)
        self.assertEqual(a, [0, 1, 4, 9, 16])

    def testISliceInput(self):
        a = IntegerList()
        b = [1, 2, 3, 4, 5]
        from itertools import islice
        a.extend(islice(b, 2, 7))
        self.assertEqual(a, [3, 4, 5])

    def testOtherListInput(self):
        a = IntegerList.fromValues((0, 1, 2))
        b = IntegerList.fromValues((3, 4, 5))
        a.extend(b)
        self.assertEqual(a, [0, 1, 2, 3, 4, 5])

    def testInvalidInput(self):
        a = IntegerList()
        with self.assertRaises(TypeError):
            a.extend("abc")
        with self.assertRaises(TypeError):
            a.extend(0)

    def testExtendVector(self):
        a = IntegerList()
        a.extend(Vector((0, 1, 2)))
        self.assertEqual(a, [0, 1, 2])

class TestMultiply(TestCase):
    def testLeft(self):
        a = IntegerList.fromValues((0, 1, 2))
        b = a * 3
        self.assertEqual(b, (0, 1, 2, 0, 1, 2, 0, 1, 2))

    def testRight(self):
        a = IntegerList.fromValues((0, 1, 2))
        b = 3 * a
        self.assertEqual(b, (0, 1, 2, 0, 1, 2, 0, 1, 2))

    def testNegativeFactor(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        b = a * (-2)
        self.assertEqual(b, [])

class TestRepeated(TestCase):
    def testAmount(self):
        a = IntegerList.fromValues((0, 1, 2))
        b = a.repeated(amount = 3)
        self.assertEqual(b, (0, 1, 2, 0, 1, 2, 0, 1, 2))

    def testLength(self):
        a = IntegerList.fromValues((0, 1, 2, 3, 4))
        b = a.repeated(length = 12)
        self.assertEqual(b, (0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1))

class TestDeleteElement(TestCase):
    def testLengthUpdate(self):
        a = IntegerList.fromValues((0, 1, 2))
        del a[1]
        self.assertEqual(len(a), 2)

    def testStart(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        del a[0]
        self.assertEqual(a, [1, 2, 3])

    def testEnd(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        del a[3]
        self.assertEqual(a, [0, 1, 2])

    def testMiddle(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        del a[1]
        self.assertEqual(a, [0, 2, 3])

    def testNegativeIndex(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        del a[-1]
        self.assertEqual(a, [0, 1, 2])
        del a[-2]
        self.assertEqual(a, [0, 2])

    def testIndexError(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        with self.assertRaises(IndexError):
            del a[10]
        with self.assertRaises(IndexError):
            del a[-10]

class TestDeleteSlice(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 2, 3, 4, 5, 6, 7))

    def testStart(self):
        del self.list[:4]
        self.assertEqual(self.list, [4, 5, 6, 7])

    def testEnd(self):
        del self.list[4:]
        self.assertEqual(self.list, [0, 1, 2, 3])

    def testMiddle(self):
        del self.list[2:6]
        self.assertEqual(self.list, [0, 1, 6, 7])

    def testStep(self):
        del self.list[::2]
        self.assertEqual(self.list, [1, 3, 5, 7])

    def testNegativeStep(self):
        del self.list[::-2]
        self.assertEqual(self.list, [0, 2, 4, 6])

    def testCombined(self):
        del self.list[1:-2:3]
        self.assertEqual(self.list, [0, 2, 3, 5, 6, 7])

class TestGetSlice(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 2, 3, 4, 5))

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.list["abc"]

    def testStart(self):
        self.assertEqual(self.list[:3], [0, 1, 2])

    def testEnd(self):
        self.assertEqual(self.list[2:], [2, 3, 4, 5])

    def testMiddle(self):
        self.assertEqual(self.list[2:4], [2, 3])

    def testStep(self):
        self.assertEqual(self.list[::2], [0, 2, 4])

    def testReverse(self):
        self.assertEqual(self.list[::-1], [5, 4, 3, 2, 1, 0])

    def testNegativeIndex(self):
        self.assertEqual(self.list[-4:-2], [2, 3])
        self.assertEqual(self.list[-1:-5:-1], [5, 4, 3, 2])

class TestGetIndexList(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues([3, 6, 7, 10, 4])

    def testEmpty(self):
        self.assertEqual(self.list[tuple()], [])

    def testNormal(self):
        self.assertEqual(self.list[(0, 1, 3)], [3, 6, 10])

    def testNegativeIndex(self):
        self.assertEqual(self.list[(1, -2, -3)], [6, 10, 7])

    def testList(self):
        self.assertEqual(self.list[[3, 2, 1]], [10, 7, 6])

    def testIndexError(self):
        with self.assertRaises(IndexError):
            a = self.list[(3, 10, 2)]
        with self.assertRaises(IndexError):
            a = self.list[(1, -10, -2)]

    def testInvalidType(self):
        with self.assertRaises(TypeError):
            a = self.list["abc"]
        with self.assertRaises(TypeError):
            a = self.list[("fsd", 3, 4)]

class TestRemove(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 2, 3, 2, 3))

    def testNotExists(self):
        with self.assertRaises(ValueError):
            self.list.remove(10)

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.list.remove("abc")

    def testStart(self):
        self.list.remove(0)
        self.assertEqual(self.list, [1, 2, 3, 2, 3])

    def testLengthUpdate(self):
        self.assertEqual(len(self.list), 6)
        self.list.remove(1)
        self.assertEqual(len(self.list), 5)

    def testOnlyRemoveFirst(self):
        self.list.remove(2)
        self.assertEqual(self.list, [0, 1, 3, 2, 3])

class TestCount(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 2, 3, 1, 3, 2, 4, 3))

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.list.count("abc")

    def testNormal(self):
        self.assertEqual(self.list.count(0), 1)
        self.assertEqual(self.list.count(1), 2)
        self.assertEqual(self.list.count(2), 2)
        self.assertEqual(self.list.count(3), 3)
        self.assertEqual(self.list.count(4), 1)

    def testNotExist(self):
        self.assertEqual(self.list.count(10), 0)

class TestIndex(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 0, 0, 1, 2, 1, 3, 3, 4))

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.list.index("abc")

    def testNormal(self):
        self.assertEqual(self.list.index(0), 0)
        self.assertEqual(self.list.index(1), 1)
        self.assertEqual(self.list.index(2), 5)
        self.assertEqual(self.list.index(3), 7)
        self.assertEqual(self.list.index(4), 9)

    def testNotExist(self):
        with self.assertRaises(ValueError):
            self.list.index(10)

class TestFill(TestCase):
    def testUninitialized(self):
        a = IntegerList(5)
        a.fill(2)
        self.assertEqual(a, [2, 2, 2, 2, 2])

    def testInitialized(self):
        a = IntegerList.fromValues((4, 23, 6, 2, 3))
        a.fill(-1)
        self.assertEqual(a, [-1, -1, -1, -1, -1])

    def testWrongType(self):
        a = IntegerList(4)
        with self.assertRaises(TypeError):
            a.fill("abc")

    def testEmptyList(self):
        a = IntegerList()
        a.fill(4)
        self.assertEqual(a, [])

class TestReversed(TestCase):
    def testEmptyList(self):
        a = IntegerList()
        result = a.reversed()
        self.assertEqual(result, [])

    def testNormal(self):
        a = IntegerList.fromValues((0, 1, 2, 3, 4))
        result = a.reversed()
        self.assertEqual(result, [4, 3, 2, 1, 0])

class TestContains(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 1, 3, 4, 0, 5))

    def testContains(self):
        self.assertTrue(0 in self.list)
        self.assertTrue(1 in self.list)
        self.assertTrue(3 in self.list)
        self.assertTrue(4 in self.list)
        self.assertTrue(5 in self.list)

    def testContainsNot(self):
        self.assertFalse(2 in self.list)
        self.assertFalse(-1 in self.list)
        self.assertFalse(10 in self.list)

    def testNotIn(self):
        self.assertFalse(1 not in self.list)
        self.assertTrue(10 not in self.list)

class TestSetElement(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 2, 3, 4))

    def testStart(self):
        self.list[0] = 10
        self.assertEqual(self.list, [10, 1, 2, 3, 4])

    def testEnd(self):
        self.list[4] = 10
        self.assertEqual(self.list, [0, 1, 2, 3, 10])

    def testMiddle(self):
        self.list[2] = 10
        self.assertEqual(self.list, [0, 1, 10, 3, 4])

    def testNegativeIndex(self):
        self.list[-2] = 10
        self.assertEqual(self.list, [0, 1, 2, 10, 4])

class TestSetSlice(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues((0, 1, 2, 3, 4))

    def testStart(self):
        self.list[:3] = [6, 7, 8]
        self.assertEqual(self.list, [6, 7, 8, 3, 4])

    def testEnd(self):
        self.list[2:] = [7, 8, 9]
        self.assertEqual(self.list, [0, 1, 7, 8, 9])

    def testMiddle(self):
        self.list[1:4] = [6, 7, 8]
        self.assertEqual(self.list, [0, 6, 7, 8, 4])

    def testStep(self):
        self.list[1::2] = [6, 7]
        self.assertEqual(self.list, [0, 6, 2, 7, 4])

    def testInputTypes(self):
        self.list[2:5] = (6, 7, 8)
        self.assertEqual(self.list, [0, 1, 6, 7, 8])
        self.list[2:5] = [0, 1, 2]
        self.assertEqual(self.list, [0, 1, 0, 1, 2])
        self.list[2:5] = IntegerList.fromValues((6, 7, 8))
        self.assertEqual(self.list, [0, 1, 6, 7, 8])
        self.list[2:5] = FloatList.fromValues((0, 1, 2))
        self.assertEqual(self.list, [0, 1, 0, 1, 2])

    def testWrongLengthExtendedSlice(self):
        with self.assertRaises(ValueError):
            self.list[::2] = [1, 2, 3, 4, 5]

    def testShorterReplacement(self):
        self.list[:3] = [10]
        self.assertEqual(self.list, [10, 3, 4])

    def testLongerReplacement(self):
        self.list[2:4] = [6, 7, 8, 9, 10]
        self.assertEqual(self.list, [0, 1, 6, 7, 8, 9, 10, 4])

    def testInsertion(self):
        self.list[2:2] = [6, 7, 8, 9]
        self.assertEqual(self.list, [0, 1, 6, 7, 8, 9, 2, 3, 4])

    def testDelete(self):
        self.list[1:] = []
        self.assertEqual(self.list, [0])

class TestFromValues(TestCase):
    def testList(self):
        a = IntegerList.fromValues([0, 1, 2, 3])
        self.assertEqual(a, [0, 1, 2, 3])

    def testTuple(self):
        a = IntegerList.fromValues((0, 1, 2, 3))
        self.assertEqual(a, [0, 1, 2, 3])

    def testSameType(self):
        tmp = IntegerList.fromValues([0, 1, 2, 3])
        a = IntegerList.fromValues(tmp)
        self.assertEqual(a, [0, 1, 2, 3])

    def testOtherType(self):
        tmp = FloatList.fromValues([0, 1, 2, 3])
        a = IntegerList.fromValues(tmp)
        self.assertEqual(a, [0, 1, 2, 3])

    def testWrongType(self):
        with self.assertRaises(TypeError):
            a = IntegerList.fromValues("abc")

    def testVector(self):
        a = IntegerList.fromValues(Vector((0, 1, 2)))
        self.assertEqual(a, [0, 1, 2])

    def testGeneratorInput(self):
        a = IntegerList.fromValues(range(7))
        self.assertEqual(a, [0, 1, 2, 3, 4, 5, 6])

class TestAdd(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues([0, 1, 2, 3])

    def testSameType(self):
        result = self.list + IntegerList.fromValues([4, 5, 6, 7])
        self.assertEqual(result, [0, 1, 2, 3, 4, 5, 6, 7])

    def testListLeft(self):
        result = [6, 7, 8] + self.list
        self.assertEqual(result, [6, 7, 8, 0, 1, 2, 3])

    def testListRight(self):
        result = self.list + [6, 7, 8]
        self.assertEqual(result, [0, 1, 2, 3, 6, 7, 8])

    def testTestMultiple(self):
        result = [6, 7] + [5, 6] + self.list + [0, 9, 8] + [1, 2]
        self.assertEqual(result, [6, 7, 5, 6, 0, 1, 2, 3, 0, 9, 8, 1, 2])

    def testTuple(self):
        result = self.list + (8, 9)
        self.assertEqual(result, [0, 1, 2, 3, 8, 9])

    def testWrongType(self):
        with self.assertRaises(NotImplementedError):
            result = self.list + [1, 2, "abc"]

class TestIAdd(TestCase):
    def setUp(self):
        self.list = IntegerList.fromValues([0, 1, 2, 3])

    def testWrongType(self):
        with self.assertRaises(NotImplementedError):
            self.list += [1, 2, "34"]

    def testSameType(self):
        self.list += IntegerList.fromValues([4, 5, 6])
        self.assertEqual(self.list, [0, 1, 2, 3, 4, 5, 6])

    def testList(self):
        self.list += [4, 5, 6]
        self.assertEqual(self.list, [0, 1, 2, 3, 4, 5, 6])

    def testTuple(self):
        self.list += (4, 5, 6)
        self.assertEqual(self.list, [0, 1, 2, 3, 4, 5, 6])

    def testSimiliarType(self):
        self.list += FloatList.fromValues([4, 5, 6])
        self.assertEqual(self.list, [0, 1, 2, 3, 4, 5, 6])
