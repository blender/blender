The following 4 steps to adding a new image format to blender, its
probably easiest to look at the png code for a clean clear example,
animation formats are a bit more complicated but very similar:

Step 1:
create a new file named after the format for example lets say we were
creating an openexr read/writer  use openexr.c
It should contain functions to match the following prototypes:

struct ImBuf *imb_loadopenexr(unsigned char *mem,int size,int flags);
/* Use one of the following depending on whats easyer for your file format */
short imb_saveopenexr(struct ImBuf *ibuf, FILE myfile, int flags);
short imb_saveopenexr(struct ImBuf *ibuf, char *myfile, int flags);

/* Used to test if its the correct format
int IMB_is_openexr(void *buf);

Step 2: 
Add your hooks to read and write the image format these go in
	writeimage.c and readimage.c  just look at how the others are done

Step 3: 
Add in IS_openexr to blender/source/blender/imbuf/IMB_imbuf_types.h
Add in R_openexr to source/blender/makesdna/DNA_scene_types.h

Step 4:
Add your hooks to the gui.
source/blender/src/buttons_scene.c
source/blender/src/toets.c
source/blender/src/writeimage.c

Step 5:
edit the following files:
blender/source/blender/imbuf/intern/util.c
blender/source/blender/src/filesel.c
blender/source/blender/src/screendump.c
and add your extension so that your format gets recognized in the thumbnails.

Step 6: 
Alter the build process:
For scons you need to edit blender/source/blender/imbuf/SConscript 
and add in your additional files to source_files.
For msvp you need to edit blender/projectfiles/blender/imbuf/BL_imbuf.dsp
and add in your additional files.
If you have any external library info you will also need to add that 
to the various build processes.

Step 7:
Its also good to add your image format to:
makepicstring in blender/source/blender/blenkernel/intern/image.c
