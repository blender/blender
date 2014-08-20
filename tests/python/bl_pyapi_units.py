# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_pyapi_units.py
import unittest
from test import support

from bpy.utils import units

class UnitsTesting(unittest.TestCase):
    # From user typing to 'internal' Blender value.
    INPUT_TESTS = (
        # system,    type,    ref, input, value
        ##### LENGTH
        ('IMPERIAL', 'LENGTH', "", "1ft", 0.3048),
        ('IMPERIAL', 'LENGTH', "", "(1+1)ft", 0.3048 * 2),
        ('IMPERIAL', 'LENGTH', "", "1mi4\"", 1609.344 + 0.0254 * 4),
        ('METRIC',   'LENGTH', "", "0.005µm", 0.000001 * 0.005),
        ('METRIC',   'LENGTH', "", "1e6km", 1000.0 * 1e6),
        ('IMPERIAL', 'LENGTH', "", "1ft5cm", 0.3048 + 0.01 * 5),
        ('METRIC',   'LENGTH', "", "1ft5cm", 0.3048 + 0.01 * 5),
        # Using reference string to find a unit when none is given.
        ('IMPERIAL', 'LENGTH', "33.3ft", "1", 0.3048),
        ('METRIC',   'LENGTH', "33.3dm", "1", 0.1),
        ('IMPERIAL', 'LENGTH', "33.3cm", "1", 0.3048),  # ref unit is not in IMPERIAL system, default to feet...
        ('IMPERIAL', 'LENGTH', "33.3ft", "1\"", 0.0254),  # unused ref unit, since one is given already!
        ('IMPERIAL', 'LENGTH', "", "1+1ft", 0.3048 * 2),  # default unit taken from current string (feet).
        ('METRIC',   'LENGTH', "", "1+1ft", 1.3048),  # no metric units, we default to meters.
        ('IMPERIAL', 'LENGTH', "", "3+1in+1ft", 0.3048 * 4 + 0.0254),  # bigger unit becomes default one!
        ('IMPERIAL', 'LENGTH', "", "(3+1)in+1ft", 0.3048 + 0.0254 * 4),
    )

    # From 'internal' Blender value to user-friendly printing
    OUTPUT_TESTS = (
        # system,    type,    prec, sep, compat, value, output
        ##### LENGTH
        ('IMPERIAL', 'LENGTH', 3, False, False, 0.3048, "1'"),
        ('IMPERIAL', 'LENGTH', 3, False, True,  0.3048, "1ft"),
        ('IMPERIAL', 'LENGTH', 3, True,  False, 0.3048 * 2 + 0.0254 * 5.5, "2' 5.5\""),
        # Those next two fail, here again because precision ignores order magnitude :/
        #('IMPERIAL', 'LENGTH', 3, False, False, 1609.344 * 1e6, "1000000mi"), # == 1000000.004mi!!!
        #('IMPERIAL', 'LENGTH', 6, False, False, 1609.344 * 1e6, "1000000mi"), # == 1000000.003641mi!!!
        ('METRIC',   'LENGTH', 3, True,  False, 1000 * 2 + 0.001 * 15, "2km 1.5cm"),
        ('METRIC',   'LENGTH', 3, True,  False, 1234.56789, "1km 234.6m"),
        # Note: precision seems basically unused when using multi units!
        ('METRIC',   'LENGTH', 9, True,  False, 1234.56789, "1km 234.6m"),
        ('METRIC',   'LENGTH', 9, False, False, 1234.56789, "1.23456789km"),
        ('METRIC',   'LENGTH', 9, True,  False, 1000.000123456789, "1km 0.1mm"),
    )

    def test_units_inputs(self):
        # Stolen from FBX addon!
        def similar_values(v1, v2, e):
            if v1 == v2:
                return True
            return ((abs(v1 - v2) / max(abs(v1), abs(v2))) <= e)

        for usys, utype, ref, inpt, val in self.INPUT_TESTS:
            opt_val = units.to_value(usys, utype, inpt, ref)
            # Note: almostequal is not good here, precision is fixed on decimal digits, not variable with
            # magnitude of numbers (i.e. 1609.4416 ~= 1609.4456 fails even at 5 of 'places'...).
            self.assertTrue(similar_values(opt_val, val, 1e-7),
                            msg="%s, %s: \"%s\" (ref: \"%s\") => %f, expected %f"
                                "" % (usys, utype, inpt, ref, opt_val, val))

    def test_units_outputs(self):
        for usys, utype, prec, sep, compat, val, output in self.OUTPUT_TESTS:
            opt_str = units.to_string(usys, utype, val, prec, sep, compat)
            self.assertEqual(opt_str, output,
                            msg="%s, %s: %f (precision: %d, separate units: %d, compat units: %d) => "
                                "\"%s\", expected \"%s\"" % (usys, utype, val, prec, sep, compat, opt_str, output))


def test_main():
    try:
        support.run_unittest(UnitsTesting)
    except:
        import traceback
        traceback.print_exc()

        # alert CTest we failed
        import sys
        sys.exit(1)

if __name__ == '__main__':
    test_main()
