"""CLI interface to BAM-pack.

Run this using:

python -m bam.pack
"""

if __name__ == '__main__':
    from bam.blend import blendfile_pack
    blendfile_pack.main()
