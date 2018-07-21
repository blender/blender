#!/usr/bin/env python3
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

# <pep8 compliant>

import argparse
import functools
import shutil
import pathlib
import subprocess
import sys
import tempfile
import unittest

from modules.test_utils import (with_tempdir,
                                AbstractBlenderRunnerTest,
                                )


class AbcPropError(Exception):
    """Raised when AbstractAlembicTest.abcprop() finds an error."""


class AbstractAlembicTest(AbstractBlenderRunnerTest):
    @classmethod
    def setUpClass(cls):
        import re

        cls.blender = args.blender
        cls.testdir = pathlib.Path(args.testdir)
        cls.alembic_root = pathlib.Path(args.alembic_root)

        # 'abcls' outputs ANSI colour codes, even when stdout is not a terminal.
        # See https://github.com/alembic/alembic/issues/120
        cls.ansi_remove_re = re.compile(rb'\x1b[^m]*m')

        # 'abcls' array notation, like "name[16]"
        cls.abcls_array = re.compile(r'^(?P<name>[^\[]+)(\[(?P<arraysize>\d+)\])?$')

    def abcprop(self, filepath: pathlib.Path, proppath: str) -> dict:
        """Uses abcls to obtain compound property values from an Alembic object.

        A dict of subproperties is returned, where the values are Python values.

        The Python bindings for Alembic are old, and only compatible with Python 2.x,
        so that's why we can't use them here, and have to rely on other tooling.
        """
        import collections

        abcls = self.alembic_root / 'bin' / 'abcls'

        command = (str(abcls), '-vl', '%s%s' % (filepath, proppath))
        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=30)

        coloured_output = proc.stdout
        output = self.ansi_remove_re.sub(b'', coloured_output).decode('utf8')

        # Because of the ANSI colour codes, we need to remove those first before
        # decoding to text. This means that we cannot use the universal_newlines
        # parameter to subprocess.run(), and have to do the conversion ourselves
        output = output.replace('\r\n', '\n').replace('\r', '\n')

        if proc.returncode:
            raise AbcPropError('Error %d running abcls:\n%s' % (proc.returncode, output))

        # Mapping from value type to callable that can convert a string to Python values.
        converters = {
            'bool_t': int,
            'uint8_t': int,
            'int16_t': int,
            'int32_t': int,
            'uint64_t': int,
            'float64_t': float,
            'float32_t': float,
        }

        result = {}

        # Ideally we'd get abcls to output JSON, see https://github.com/alembic/alembic/issues/121
        lines = collections.deque(output.split('\n'))
        while lines:
            info = lines.popleft()
            if not info:
                continue
            parts = info.split()
            proptype = parts[0]

            if proptype == 'CompoundProperty':
                # To read those, call self.abcprop() on it.
                continue
            if len(parts) < 2:
                raise ValueError('Error parsing result from abcprop: %s', info.strip())
            valtype_and_arrsize, name_and_extent = parts[1:]

            # Parse name and extent
            m = self.abcls_array.match(name_and_extent)
            if not m:
                self.fail('Unparsable name/extent from abcls: %s' % name_and_extent)
            name, extent = m.group('name'), m.group('arraysize')

            if extent != '1':
                self.fail('Unsupported extent %s for property %s/%s' % (extent, proppath, name))

            # Parse type
            m = self.abcls_array.match(valtype_and_arrsize)
            if not m:
                self.fail('Unparsable value type from abcls: %s' % valtype_and_arrsize)
            valtype, scalarsize = m.group('name'), m.group('arraysize')

            # Convert values
            try:
                conv = converters[valtype]
            except KeyError:
                self.fail('Unsupported type %s for property %s/%s' % (valtype, proppath, name))

            def convert_single_line(linevalue):
                try:
                    if scalarsize is None:
                        return conv(linevalue)
                    else:
                        return [conv(v.strip()) for v in linevalue.split(',')]
                except ValueError as ex:
                    return str(ex)

            if proptype == 'ScalarProperty':
                value = lines.popleft()
                result[name] = convert_single_line(value)
            elif proptype == 'ArrayProperty':
                arrayvalue = []
                # Arrays consist of a variable number of items, and end in a blank line.
                while True:
                    linevalue = lines.popleft()
                    if not linevalue:
                        break
                    arrayvalue.append(convert_single_line(linevalue))
                result[name] = arrayvalue
            else:
                self.fail('Unsupported type %s for property %s/%s' % (proptype, proppath, name))

        return result

    def assertAlmostEqualFloatArray(self, actual, expect, places=6, delta=None):
        """Asserts that the arrays of floats are almost equal."""

        self.assertEqual(len(actual), len(expect),
                         'Actual array has %d items, expected %d' % (len(actual), len(expect)))

        for idx, (act, exp) in enumerate(zip(actual, expect)):
            self.assertAlmostEqual(act, exp, places=places, delta=delta,
                                   msg='%f != %f at index %d' % (act, exp, idx))


class HierarchicalAndFlatExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_hierarchical_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'cubes_hierarchical.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=True, visible_layers_only=True, flatten=False)" % abc.as_posix()
        self.run_blender('cubes-hierarchy.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Cube/Cube_002/Cube_012/.xform')
        self.assertEqual(1, xform['.inherits'])
        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [1.0, 0.0, 0.0, 0.0,
             0.0, 1.0, 0.0, 0.0,
             0.0, 0.0, 1.0, 0.0,
             3.07484, -2.92265, 0.0586434, 1.0]
        )

    @with_tempdir
    def test_flat_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'cubes_flat.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=True, visible_layers_only=True, flatten=True)" % abc.as_posix()
        self.run_blender('cubes-hierarchy.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Cube_012/.xform')
        self.assertEqual(0, xform['.inherits'])

        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [0.343134, 0.485243, 0.804238, 0,
             0.0, 0.856222, -0.516608, 0,
             -0.939287, 0.177266, 0.293799, 0,
             1, 3, 4, 1],
        )


class DupliGroupExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_hierarchical_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'dupligroup_hierarchical.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=True, visible_layers_only=True, flatten=False)" % abc.as_posix()
        self.run_blender('dupligroup-scene.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Real_Cube/Linked_Suzanne/Cylinder/Suzanne/.xform')
        self.assertEqual(1, xform['.inherits'])
        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [1.0, 0.0, 0.0, 0.0,
             0.0, 1.0, 0.0, 0.0,
             0.0, 0.0, 1.0, 0.0,
             0.0, 2.0, 0.0, 1.0]
        )

    @with_tempdir
    def test_flat_export(self, tempdir: pathlib.Path):
        abc = tempdir / 'dupligroup_hierarchical.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=True, visible_layers_only=True, flatten=True)" % abc.as_posix()
        self.run_blender('dupligroup-scene.blend', script)

        # Now check the resulting Alembic file.
        xform = self.abcprop(abc, '/Suzanne/.xform')
        self.assertEqual(0, xform['.inherits'])

        self.assertAlmostEqualFloatArray(
            xform['.vals'],
            [1.5, 0.0, 0.0, 0.0,
             0.0, 1.5, 0.0, 0.0,
             0.0, 0.0, 1.5, 0.0,
             2.0, 3.0, 0.0, 1.0]
        )


class CurveExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_export_single_curve(self, tempdir: pathlib.Path):
        abc = tempdir / 'single-curve.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=True, visible_layers_only=True, flatten=False)" % abc.as_posix()
        self.run_blender('single-curve.blend', script)

        # Now check the resulting Alembic file.
        abcprop = self.abcprop(abc, '/NurbsCurve/NurbsCurveShape/.geom')
        self.assertEqual(abcprop['.orders'], [4])

        abcprop = self.abcprop(abc, '/NurbsCurve/NurbsCurveShape/.geom/.userProperties')
        self.assertEqual(abcprop['blender:resolution'], 10)


class HairParticlesExportTest(AbstractAlembicTest):
    """Tests exporting with/without hair/particles.

    Just a basic test to ensure that the enabling/disabling works, and that export
    works at all. NOT testing the quality/contents of the exported file.
    """

    def _do_test(self, tempdir: pathlib.Path, export_hair: bool, export_particles: bool) -> pathlib.Path:
        abc = tempdir / 'hair-particles.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=True, visible_layers_only=True, flatten=False, " \
                 "export_hair=%r, export_particles=%r, as_background_job=False)" \
                 % (abc.as_posix(), export_hair, export_particles)
        self.run_blender('hair-particles.blend', script)
        return abc

    @with_tempdir
    def test_with_both(self, tempdir: pathlib.Path):
        abc = self._do_test(tempdir, True, True)

        abcprop = self.abcprop(abc, '/Suzanne/Hair system/.geom')
        self.assertIn('nVertices', abcprop)

        abcprop = self.abcprop(abc, '/Suzanne/Non-hair particle system/.geom')
        self.assertIn('.velocities', abcprop)

        abcprop = self.abcprop(abc, '/Suzanne/SuzanneShape/.geom')
        self.assertIn('.faceIndices', abcprop)

    @with_tempdir
    def test_with_hair_only(self, tempdir: pathlib.Path):
        abc = self._do_test(tempdir, True, False)

        abcprop = self.abcprop(abc, '/Suzanne/Hair system/.geom')
        self.assertIn('nVertices', abcprop)

        self.assertRaises(AbcPropError, self.abcprop, abc,
                          '/Suzanne/Non-hair particle system/.geom')

        abcprop = self.abcprop(abc, '/Suzanne/SuzanneShape/.geom')
        self.assertIn('.faceIndices', abcprop)

    @with_tempdir
    def test_with_particles_only(self, tempdir: pathlib.Path):
        abc = self._do_test(tempdir, False, True)

        self.assertRaises(AbcPropError, self.abcprop, abc, '/Suzanne/Hair system/.geom')

        abcprop = self.abcprop(abc, '/Suzanne/Non-hair particle system/.geom')
        self.assertIn('.velocities', abcprop)

        abcprop = self.abcprop(abc, '/Suzanne/SuzanneShape/.geom')
        self.assertIn('.faceIndices', abcprop)

    @with_tempdir
    def test_with_neither(self, tempdir: pathlib.Path):
        abc = self._do_test(tempdir, False, False)

        self.assertRaises(AbcPropError, self.abcprop, abc, '/Suzanne/Hair system/.geom')
        self.assertRaises(AbcPropError, self.abcprop, abc,
                          '/Suzanne/Non-hair particle system/.geom')

        abcprop = self.abcprop(abc, '/Suzanne/SuzanneShape/.geom')
        self.assertIn('.faceIndices', abcprop)


class LongNamesExportTest(AbstractAlembicTest):
    @with_tempdir
    def test_export_long_names(self, tempdir: pathlib.Path):
        abc = tempdir / 'long-names.abc'
        script = "import bpy; bpy.ops.wm.alembic_export(filepath='%s', start=1, end=1, " \
                 "renderable_only=False, visible_layers_only=False, flatten=False)" % abc.as_posix()
        self.run_blender('long-names.blend', script)

        name_parts = [
            'foG9aeLahgoh5goacee1dah6Hethaghohjaich5pasizairuWigee1ahPeekiGh',
            'yoNgoisheedah2ua0eigh2AeCaiTee5bo0uphoo7Aixephah9racahvaingeeH4',
            'zuthohnoi1thooS3eezoo8seuph2Boo5aefacaethuvee1aequoonoox1sookie',
            'wugh4ciTh3dipiepeequait5uug7thiseek5ca7Eijei5ietaizokohhaecieto',
            'up9aeheenein9oteiX6fohP3thiez6Ahvah0oohah1ep2Eesho4Beboechaipoh',
            'coh4aehiacheTh0ue0eegho9oku1lohl4loht9ohPoongoow7dasiego6yimuis',
            'lohtho8eigahfeipohviepajaix4it2peeQu6Iefee1nevihaes4cee2soh4noy',
            'kaht9ahv0ieXaiyih7ohxe8bah7eeyicahjoa2ohbu7Choxua7oongah6sei4bu',
            'deif0iPaechohkee5nahx6oi2uJeeN7ze3seunohJibe4shai0mah5Iesh3Quai',
            'ChohDahshooNee0NeNohthah0eiDeese3Vu6ohShil1Iey9ja0uebi2quiShae6',
            'Dee1kai7eiph2ahh2nufah3zai3eexeengohQue1caj0eeW0xeghi3eshuadoot',
            'aeshiup3aengajoog0AhCoo5tiu3ieghaeGhie4Tu1ohh1thee8aepheingah1E',
            'ooRa6ahciolohshaifoopeo9ZeiGhae2aech4raisheiWah9AaNga0uas9ahquo',
            'thaepheip2aip6shief4EaXopei8ohPo0ighuiXah2ashowai9nohp4uach6Mei',
            'ohph4yaev3quieji3phophiem3OoNuisheepahng4waithae3Naichai7aw3noo',
            'aibeawaneBahmieyuph8ieng8iopheereeD2uu9Uyee5bei2phahXeir8eeJ8oo',
            'ooshahphei2hoh3uth5chaen7ohsai6uutiesucheichai8ungah9Gie1Aiphie',
            'eiwohchoo7ere2iebohn4Aapheichaelooriiyaoxaik7ooqua7aezahx0aeJei',
            'Vah0ohgohphiefohTheshieghichaichahch5moshoo0zai5eeva7eisi4yae8T',
            'EibeeN0fee0Gohnguz8iec6yeigh7shuNg4eingu3siph9joucahpeidoom4ree',
            'iejiu3shohheeZahHusheimeefaihoh5eecachu5eeZie9ceisugu9taidohT3U',
            'eex6dilakaix5Eetai7xiCh5Jaa8aiD4Ag3tuij1aijohv5fo0heevah8hohs3m',
            'ohqueeNgahraew6uraemohtoo5qua3oojiex6ohqu6Aideibaithaiphuriquie',
            'cei0eiN4Shiey7Aeluy3unohboo5choiphahc2mahbei5paephaiKeso1thoog1',
            'ieghif4ohKequ7ong0jah5ooBah0eiGh1caechahnahThae9Shoo0phopashoo4',
            'roh9er3thohwi5am8iequeequuSh3aic0voocai3ihi5nie2abahphupiegh7vu',
            'uv3Quei7wujoo5beingei2aish5op4VaiX0aebai7iwoaPee5pei8ko9IepaPig',
            'co7aegh5beitheesi9lu7jeeQu3johgeiphee9cheichi8aithuDehu2gaeNein',
            'thai3Tiewoo4nuir1ohy4aithiuZ7shae1luuwei5phibohriepe2paeci1Ach8',
            'phoi3ribah7ufuvoh8eigh1oB6deeBaiPohphaghiPieshahfah5EiCi3toogoo',
            'aiM8geil7ooreinee4Cheiwea4yeec8eeshi7Sei4Shoo3wu6ohkaNgooQu1mai',
            'agoo3faciewah9ZeesiXeereek7am0eigaeShie3Tisu8haReeNgoo0ci2Hae5u',
            'Aesatheewiedohshaephaenohbooshee8eu7EiJ8isal1laech2eiHo0noaV3ta',
            'liunguep3ooChoo4eir8ahSie8eenee0oo1TooXu8Cais8Aimo4eir6Phoo3xei',
            'toe9heepeobein3teequachemei0Cejoomef9ujie3ohwae9AiNgiephi3ep0de',
            'ua6xooY9uzaeB3of6sheiyaedohoiS5Eev0Aequ9ahm1zoa5Aegh3ooz9ChahDa',
            'eevasah6Bu9wi7EiwiequumahkaeCheegh6lui8xoh4eeY4ieneavah8phaibun',
            'AhNgei2sioZeeng6phaecheemeehiShie5eFeiTh6ooV8iiphabud0die4siep4',
            'kushe6Xieg6ahQuoo9aex3aipheefiec1esa7OhBuG0ueziep9phai5eegh1vie',
            'Jie5yu8aafuQuoh9shaep3moboh3Pooy7och8oC6obeik6jaew2aiLooweib3ch',
            'ohohjajaivaiRail3odaimei6aekohVaicheip2wu7phieg5Gohsaing2ahxaiy',
            'hahzaht6yaiYu9re9jah9loisiit4ahtoh2quoh9xohishioz4oo4phofu3ogha',
            'pu4oorea0uh2tahB8aiZoonge1aophaes6ogaiK9ailaigeej4zoVou8ielotee',
            'cae2thei3Luphuqu0zeeG8leeZuchahxaicai4ui4Eedohte9uW6gae8Geeh0ea',
            'air7tuy7ohw5sho2Tahpai8aep4so5ria7eaShus5weaqu0Naquei2xaeyoo2ae',
            'vohge4aeCh7ahwoo7Jaex6sohl0Koong4Iejisei8Coir0iemeiz9uru9Iebaep',
            'aepeidie8aiw6waish9gie4Woolae2thuj5phae4phexux7gishaeph4Deu7ooS',
            'vahc5ia0xohHooViT0uyuxookiaquu2ogueth0ahquoudeefohshai8aeThahba',
            'mun3oagah2eequaenohfoo8DaigeghoozaV2eiveeQuee7kah0quaa6tiesheet',
            'ooSet4IdieC4ugow3za0die4ohGoh1oopoh6luaPhaeng4Eechea1hae0eimie5',
            'iedeimadaefu2NeiPaey2jooloov5iehiegeakoo4ueso7aeK9ahqu2Thahkaes',
            'nahquah9Quuu2uuf0aJah7eishi2siegh8ue5eiJa2EeVu8ebohkepoh4dahNgo',
            'io1bie7chioPiej5ae2oohe2fee6ooP2thaeJohjohb9Se8tang3eipaifeimai',
            'oungoqu6dieneejiechez1xeD2Zi9iox2Ahchaiy9ithah3ohVoolu2euQuuawo',
            'thaew0veigei4neishohd8mecaixuqu7eeshiex1chaigohmoThoghoitoTa0Eo',
            'ahroob2phohvaiz0Ohteik2ohtakie6Iu1vitho8IyiyeeleeShae9defaiw9ki',
            'DohHoothohzeaxolai3Toh5eJie7ahlah9reF0ohn1chaipoogain2aibahw4no',
            'aif8lo5she4aich5cho2rie8ieJaujeem2Joongeedae4vie3tah1Leequaix1O',
            'Aang0Shaih6chahthie1ahZ7aewei9thiethee7iuThah3yoongi8ahngiobaa5',
            'iephoBuayoothah0Ru6aichai4aiw8deg1umongauvaixai3ohy6oowohlee8ei',
            'ohn5shigoameer0aejohgoh8oChohlaecho9jie6shu0ahg9Bohngau6paevei9',
            'edahghaishak0paigh1eecuich3aad7yeB0ieD6akeeliem2beifufaekee6eat',
            'hiechahgheloh2zo7Ieghaiph0phahhu8aeyuiKie1xeipheech9zai4aeme0ee',
            'Cube'
        ]
        name = '/' + '/'.join(name_parts)

        # Now check the resulting Alembic file.
        abcprop = self.abcprop(abc, '%s/.xform' % name)
        self.assertEqual(abcprop['.vals'], [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 3.0, 0.0, 1.0,
        ])

        abcprop = self.abcprop(abc, '%s/CubeShape/.geom' % name)
        self.assertIn('.faceCounts', abcprop)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--blender', required=True)
    parser.add_argument('--testdir', required=True)
    parser.add_argument('--alembic-root', required=True)
    args, remaining = parser.parse_known_args()

    unittest.main(argv=sys.argv[0:1] + remaining)
