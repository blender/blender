/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char**argv) {
	FILE *fpin,  *fpout;
	char cname[256];
	char sizest[256];
	size_t size;
	int i;
	
	if (argc<1) {
			printf ("Usage: datatoc <data_file>\n");
			exit(1);
	}
	
	fpin= fopen(argv[1], "rb");
	if (!fpin) {
			printf ("Unable to open input <%s>\n", argv[1]);
			exit(1);
	}
	
	fseek (fpin, 0L,  SEEK_END);
	size= ftell(fpin);
	fseek (fpin, 0L,  SEEK_SET);

	if (argv[1][0]=='.') argv[1]++;

	sprintf(cname, "%s.c", argv[1]);
	printf ("Making C file <%s>\n", cname);

	for (i=0; i < (int)strlen(argv[1]); i++)
		if (argv[1][i]=='.') argv[1][i]='_';

	sprintf(sizest, "%d", (int)size);
	printf ("Input filesize is %ld, Output size should be %ld\n", size, ((int)size)*4 + strlen("/* DataToC output of file <> */\n\n") + strlen("char datatoc_[]= {\"") + strlen ("\"};\n") + (strlen(argv[1])*3) + strlen(sizest) + strlen("int datatoc__size= ;\n") +(((int)(size/256)+1)*5));
	
	fpout= fopen(cname, "w");
	if (!fpout) {
			printf ("Unable to open output <%s>\n", cname);
			exit(1);
	}
	
	fprintf (fpout, "/* DataToC output of file <%s> */\n\n",argv[1]);
	fprintf (fpout, "int datatoc_%s_size= %s;\n", argv[1], sizest);
	/*
	fprintf (fpout, "char datatoc_%s[]= {\"", argv[1]);

	while (size--) {
		if(size%256==0)
			fprintf(fpout, "\" \\\n\"");
			
		fprintf (fpout, "\\x%02x", getc(fpin));
	}

	fprintf (fpout, "\"};\n");
	*/
	
	fprintf (fpout, "char datatoc_%s[]= {\n", argv[1]);
	while (size--) {
		if(size%32==31)
			fprintf(fpout, "\n");
			
		/* fprintf (fpout, "\\x%02x", getc(fpin)); */
		fprintf (fpout, "%3d,", getc(fpin));
	}
	/* null terminate for the case it is a string */
	fprintf (fpout, "\n  0};\n\n");
	
	fclose(fpin);
	fclose(fpout);
	return 0;
}
