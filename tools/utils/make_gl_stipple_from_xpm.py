# SPDX-License-Identifier: GPL-2.0-or-later

# Converts 32x32 XPM images written be the gimp to GL stipples
# takes XPM files as arguments, prints out C style definitions.

import sys
import os


def main():
    xpm_ls = [f for f in sys.argv[1:] if f.lower().endswith(".xpm")]

    print("Converting: " + " ".join(xpm_ls))

    for xpm in xpm_ls:
        f = open(xpm, "r")
        data = f.read()
        f.close()

        # all after first {
        data = data.split("{", 1)[1]

        # all before first }
        data = data.rsplit("}", 1)[0]

        data = data.replace("\n", "")

        data = data.split(",")

        w, h, c, dummy = map(int, data[0].strip("\"").split())

        if w != 32 or h != 32 or c != 2:
            print("Skipping %r, expected 32x32, monochrome, got %s" %
                  (xpm, data[0]))
            continue

        # col_1 = data[1][1]
        col_2 = data[2][1]

        data = [d[1:-1] for d in data[3:]]

        bits = []

        for d in data:
            for i, c in enumerate(d):
                bits.append('01'[(c == col_2)])

        if len(bits) != 1024:
            print("Skipping %r, expected 1024 bits, got %d" %
                  (xpm, len(bits)))
            continue

        bits = "".join(bits)

        chars = []

        for i in range(0, len(bits), 8):
            chars.append("0x%.2x" % int(bits[i:i + 8], 2))

        fout = sys.stdout

        var = os.path.basename(xpm)
        var = os.path.splitext(var)[0]

        fout.write("GLubyte stipple_%s[128] {\n\t" % var)
        for i, c in enumerate(chars):
            if i != 127:
                fout.write("%s, " % c)
            else:
                fout.write("%s" % c)

            if not ((i + 1) % 8):
                fout.write("\n\t")
        fout.write("};\n")


if __name__ == "__main__":
    main()
