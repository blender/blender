To inspect the blend-file-format used by a certain version of blender 2.5x,
navigate to this folder and run this command:

blender2.5 -b -P BlendFileDnaExporter_25.py

where "blender2.5" is your blender executable or a symlink to it.

This creates a temporary dna.blend to be inspected and it produces two new files:

* dna.html: the list of all the structures saved in a blend file with the blender2.5
            executable you have used. If you enable build information when you build blender,
            the dna.html file will also show which svn revision the html refers to.
* dna.css:  the css for the html above

Below you have the help message with a list of options you can use.


Usage:
        blender2.5 --background -noaudio --python BlendFileDnaExporter_25.py [-- [options]]
Options:
        --dna-keep-blend:      doesn't delete the produced blend file DNA export to html
        --dna-debug:           sets the logging level to DEBUG (lots of additional info)
        --dna-versioned        saves version information in the html and blend filenames
        --dna-overwrite-css    overwrite dna.css, useful when modifying css in the script
Examples:
        default:       % blender2.5 --background -noaudio --python BlendFileDnaExporter_25.py
        with options:  % blender2.5 --background -noaudio --python BlendFileDnaExporter_25.py -- --dna-keep-blend --dna-debug


