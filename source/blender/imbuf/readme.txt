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

Step 4: 
Add any external library info to the build process.

