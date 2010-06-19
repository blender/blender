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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Reorganised mar-01 nzc
 * Some really low-level file thingies.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>	

#ifndef WIN32
#include <dirent.h>
#endif

#include <time.h>
#include <sys/stat.h>

#if defined (__sun__) || defined (__sun) || defined (__sgi) || defined (__NetBSD__)
#include <sys/statvfs.h> /* Other modern unix os's should probably use this also */
#elif !defined(__FreeBSD__) && !defined(linux) && (defined(__sparc) || defined(__sparc__))
#include <sys/statfs.h>
#endif

#if defined (__FreeBSD__) || defined (__OpenBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#endif

#if defined(linux) || defined(__CYGWIN32__) || defined(__hpux)
#include <sys/vfs.h>
#endif

#ifdef __APPLE__
/* For statfs */
#include <sys/param.h>
#include <sys/mount.h>
#endif /* __APPLE__ */


#include <fcntl.h>
#include <string.h>			/* strcpy etc.. */

#ifndef WIN32
#include <sys/ioctl.h>
#include <unistd.h>			/*  */
#include <pwd.h>
#endif

#if !defined(__FreeBSD__) && !defined(__APPLE__)
#include <malloc.h>
#endif

#ifdef WIN32
#include <sys/types.h>
#include <io.h>
#include <direct.h>
#include "BLI_winstuff.h"
#endif


/* lib includes */
#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_storage_types.h"
#include "BLI_string.h"
#include "BKE_utildefines.h"

/* vars: */
static int totnum,actnum;
static struct direntry *files;

static struct ListBase dirbase_={
	0,0};
static struct ListBase *dirbase = &dirbase_;


char *BLI_getwdN(char *dir)
{
	char *pwd;

	if (dir) {
		pwd = getenv("PWD");
		if (pwd){
			strcpy(dir, pwd);
			return(dir);
		}
		/* 160 is FILE_MAXDIR in filesel.c */
		return( getcwd(dir, 160) );
	}
	return(0);
}


int BLI_compare(struct direntry *entry1, struct direntry *entry2)
{
	/* type is equal to stat.st_mode */

	if (S_ISDIR(entry1->type)){
		if (S_ISDIR(entry2->type)==0) return (-1);
	} else{
		if (S_ISDIR(entry2->type)) return (1);
	}
	if (S_ISREG(entry1->type)){
		if (S_ISREG(entry2->type)==0) return (-1);
	} else{
		if (S_ISREG(entry2->type)) return (1);
	}
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);
	
	/* make sure "." and ".." are always first */
	if( strcmp(entry1->relname, ".")==0 ) return (-1);
	if( strcmp(entry2->relname, ".")==0 ) return (1);
	if( strcmp(entry1->relname, "..")==0 ) return (-1);
	if( strcmp(entry2->relname, "..")==0 ) return (1);

	return (BLI_natstrcmp(entry1->relname,entry2->relname));
}


double BLI_diskfree(char *dir)
{
#ifdef WIN32
	DWORD sectorspc, bytesps, freec, clusters;
	char tmp[4];
	
	tmp[0]='\\'; tmp[1]=0; /* Just a failsafe */
	if (dir[0]=='/' || dir[0]=='\\') {
		tmp[0]='\\';
		tmp[1]=0;
	} else if (dir[1]==':') {
		tmp[0]=dir[0];
		tmp[1]=':';
		tmp[2]='\\';
		tmp[3]=0;
	}

	GetDiskFreeSpace(tmp,&sectorspc, &bytesps, &freec, &clusters);

	return (double) (freec*bytesps*sectorspc);
#else

#if defined (__sun__) || defined (__sun) || defined (__sgi) || defined (__NetBSD__)
	struct statvfs disk;
#else
	struct statfs disk;
#endif
	char name[FILE_MAXDIR],*slash;
	int len = strlen(dir);
	
	if (len >= FILE_MAXDIR) /* path too long */
		return -1;
	
	strcpy(name,dir);

	if(len){
		slash = strrchr(name,'/');
		if (slash) slash[1] = 0;
	} else strcpy(name,"/");

#if defined (__FreeBSD__) || defined (linux) || defined (__OpenBSD__) || defined (__APPLE__) 
	if (statfs(name, &disk)) return(-1);
#endif

#if defined (__sun__) || defined (__sun) || defined (__sgi) || defined (__NetBSD__)
	if (statvfs(name, &disk)) return(-1);	
#elif !defined(__FreeBSD__) && !defined(linux) && (defined(__sparc) || defined(__sparc__))
	/* WARNING - This may not be supported by geeneric unix os's - Campbell */
	if (statfs(name, &disk, sizeof(struct statfs), 0)) return(-1);
#endif

	return ( ((double) disk.f_bsize) * ((double) disk.f_bfree));
#endif
}

void BLI_builddir(char *dirname, char *relname)
{
	struct dirent *fname;
	struct dirlink *dlink;
	int rellen, newnum = 0, len;
	char buf[256];
	DIR *dir;

	strcpy(buf,relname);
	rellen=strlen(relname);

	if (rellen){
		buf[rellen]='/';
		rellen++;
	}

	if (chdir(dirname) == -1){
		perror(dirname);
		return;
	}

	if ( (dir = (DIR *)opendir(".")) ){
		while ((fname = (struct dirent*) readdir(dir)) != NULL) {
			len= strlen(fname->d_name);
			
			dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
			if (dlink){
				strcpy(buf+rellen,fname->d_name);
				dlink->name = BLI_strdup(buf);
				BLI_addhead(dirbase,dlink);
				newnum++;
			}
		}
		
		if (newnum){

			if (files) files=(struct direntry *)realloc(files,(totnum+newnum) * sizeof(struct direntry));
			else files=(struct direntry *)malloc(newnum * sizeof(struct direntry));

			if (files){
				dlink = (struct dirlink *) dirbase->first;
				while(dlink){
					memset(&files[actnum], 0 , sizeof(struct direntry));
					files[actnum].relname = dlink->name;
					files[actnum].path = BLI_strdupcat(dirname, dlink->name);
// use 64 bit file size, only needed for WIN32 and WIN64. 
// Excluding other than current MSVC compiler until able to test.
#if (defined(WIN32) || defined(WIN64)) && (_MSC_VER>=1500)
					_stat64(dlink->name,&files[actnum].s);
#else
					stat(dlink->name,&files[actnum].s);
#endif
					files[actnum].type=files[actnum].s.st_mode;
					files[actnum].flags = 0;
					totnum++;
					actnum++;
					dlink = dlink->next;
				}
			} else{
				printf("Couldn't get memory for dir\n");
				exit(1);
			}

			BLI_freelist(dirbase);
			if (files) qsort(files, actnum, sizeof(struct direntry), (int (*)(const void *,const void*))BLI_compare);
		} else {
			printf("%s empty directory\n",dirname);
		}

		closedir(dir);
	} else {
		printf("%s non-existant directory\n",dirname);
	}
}

void BLI_adddirstrings()
{
	char datum[100];
	char buf[512];
	char size[250];
	static char * types[8] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};
	int num, mode;
#ifdef WIN32
	__int64 st_size;
#else
	off_t st_size;
#endif
	
	struct direntry * file;
	struct tm *tm;
	time_t zero= 0;
	
	for(num=0, file= files; num<actnum; num++, file++){
#ifdef WIN32
		mode = 0;
		strcpy(file->mode1, types[0]);
		strcpy(file->mode2, types[0]);
		strcpy(file->mode3, types[0]);
#else
		mode = file->s.st_mode;

		strcpy(file->mode1, types[(mode & 0700) >> 6]);
		strcpy(file->mode2, types[(mode & 0070) >> 3]);
		strcpy(file->mode3, types[(mode & 0007)]);
		
		if (((mode & S_ISGID) == S_ISGID) && (file->mode2[2]=='-'))file->mode2[2]='l';

		if (mode & (S_ISUID | S_ISGID)){
			if (file->mode1[2]=='x') file->mode1[2]='s';
			else file->mode1[2]='S';

			if (file->mode2[2]=='x')file->mode2[2]='s';
		}

		if (mode & S_ISVTX){
			if (file->mode3[2] == 'x') file->mode3[2] = 't';
			else file->mode3[2] = 'T';
		}
#endif

#ifdef WIN32
		strcpy(file->owner,"user");
#else
		{
			struct passwd *pwuser;
			pwuser = getpwuid(file->s.st_uid);
			if ( pwuser ) {
				BLI_strncpy(file->owner, pwuser->pw_name, sizeof(file->owner));
			} else {
				snprintf(file->owner, sizeof(file->owner), "%d", file->s.st_uid);
			}
		}
#endif

		tm= localtime(&file->s.st_mtime);
		// prevent impossible dates in windows
		if(tm==NULL) tm= localtime(&zero);
		strftime(file->time, 8, "%H:%M", tm);
		strftime(file->date, 16, "%d-%b-%y", tm);

		/*
		 * Seems st_size is signed 32-bit value in *nix and Windows.  This
		 * will buy us some time until files get bigger than 4GB or until
		 * everyone starts using __USE_FILE_OFFSET64 or equivalent.
		 */
		st_size= file->s.st_size;

		if (st_size > 1024*1024*1024) {
			sprintf(file->size, "%.2f GB", ((double)st_size)/(1024*1024*1024));	
		}
		else if (st_size > 1024*1024) {
			sprintf(file->size, "%.1f MB", ((double)st_size)/(1024*1024));
		}
		else if (st_size > 1024) {
			sprintf(file->size, "%d KB", (int)(st_size/1024));
		}
		else {
			sprintf(file->size, "%d B", (int)st_size);
		}

		strftime(datum, 32, "%d-%b-%y %H:%M", tm);

		if (st_size < 1000) {
			sprintf(size, "%10d", (int) st_size);
		} else if (st_size < 1000 * 1000) {
			sprintf(size, "%6d %03d", (int) (st_size / 1000), (int) (st_size % 1000));
		} else if (st_size < 100 * 1000 * 1000) {
			sprintf(size, "%2d %03d %03d", (int) (st_size / (1000 * 1000)), (int) ((st_size / 1000) % 1000), (int) ( st_size % 1000));
		} else {
			sprintf(size, "> %4.1f M", (double) (st_size / (1024.0 * 1024.0)));
			sprintf(size, "%10d", (int) st_size);
		}

		sprintf(buf,"%s %s %s %7s %s %s %10s %s", file->mode1, file->mode2, file->mode3, file->owner, file->date, file->time, size,
			file->relname);

		file->string=MEM_mallocN(strlen(buf)+1, "filestring");
		if (file->string){
			strcpy(file->string,buf);
		}
	}
}

unsigned int BLI_getdir(char *dirname,  struct direntry **filelist)
{
	// reset global variables
	// memory stored in files is free()'d in
	// filesel.c:freefilelist()

	actnum = totnum = 0;
	files = 0;

	BLI_builddir(dirname,"");
	BLI_adddirstrings();

	if (files) {
		*(filelist) = files;
	} else {
		// keep blender happy. Blender stores this in a variable
		// where 0 has special meaning.....
		*(filelist) = files = malloc(sizeof(struct direntry));
	}

	return(actnum);
}


int BLI_filesize(int file)
{
	struct stat buf;

	if (file <= 0) return (-1);
	fstat(file, &buf);
	return (buf.st_size);
}

int BLI_filepathsize(const char *path)
{
	int size, file = open(path, O_BINARY|O_RDONLY);
	
	if (file < 0)
		return -1;
	
	size = BLI_filesize(file);
	close(file);
	return size;
}


int BLI_exist(char *name)
{
	struct stat st;
#ifdef WIN32
	/*  in Windows stat doesn't recognize dir ending on a slash 
		To not break code where the ending slash is expected we
		don't mess with the argument name directly here - elubie */
	char tmp[FILE_MAXDIR+FILE_MAXFILE];
	int len;
	BLI_strncpy(tmp, name, FILE_MAXDIR+FILE_MAXFILE);
	len = strlen(tmp);
	if (len > 3 && ( tmp[len-1]=='\\' || tmp[len-1]=='/') ) tmp[len-1] = '\0';
	if (stat(tmp,&st)) return(0);
#else
	if (stat(name,&st)) return(0);	
#endif
	return(st.st_mode);
}

/* would be better in fileops.c except that it needs stat.h so add here */
int BLI_is_dir(char *file) {
	return S_ISDIR(BLI_exist(file));
}

LinkNode *BLI_read_file_as_lines(char *name)
{
	FILE *fp= fopen(name, "r");
	LinkNode *lines= NULL;
	char *buf;
	int size;

	if (!fp) return NULL;
		
	fseek(fp, 0, SEEK_END);
	size= ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buf= MEM_mallocN(size, "file_as_lines");
	if (buf) {
		int i, last= 0;
		
			/* 
			 * size = because on win32 reading
			 * all the bytes in the file will return
			 * less bytes because of crnl changes.
			 */
		size= fread(buf, 1, size, fp);
		for (i=0; i<=size; i++) {
			if (i==size || buf[i]=='\n') {
				char *line= BLI_strdupn(&buf[last], i-last);

				BLI_linklist_prepend(&lines, line);
				last= i+1;
			}
		}
		
		MEM_freeN(buf);
	}
	
	fclose(fp);
	
	BLI_linklist_reverse(&lines);
	return lines;
}

void BLI_free_file_lines(LinkNode *lines)
{
	BLI_linklist_free(lines, (void(*)(void*)) MEM_freeN);
}

int BLI_file_older(const char *file1, const char *file2)
{
	struct stat st1, st2;

	if(stat(file1, &st1)) return 0;
	if(stat(file2, &st2)) return 0;

	return (st1.st_mtime < st2.st_mtime);
}

