/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Reorganised mar-01 nzc
 * Some really low-level file thingies.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>			/* voorkomt dat je bij malloc type moet aangeven */

#ifdef WIN32

#include <windows.h>
#include "BLI_winstuff.h"
#include <sys/types.h>
#include <io.h>
#include <direct.h>
#endif

#ifndef WIN32
#include <dirent.h>
#endif

#include <time.h>
#include <sys/stat.h>

#if defined(__sgi) || defined(__sun__)
#include <sys/statfs.h>
#endif

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/mount.h>
#endif

#if defined(linux) || defined(__CYGWIN32__)
#include <sys/vfs.h>
#endif

#ifdef __BeOS
struct statfs {
	int f_bsize;
	int f_bfree;
};
#endif

#ifdef __APPLE__
/* For statfs */
#include <sys/param.h>
#include <sys/mount.h>
#endif /* __APPLE__ */


#include <fcntl.h>
#if !defined(__BeOS) && !defined(WIN32)
#include <sys/mtio.h>			/* tape comando's */
#endif
#include <string.h>			/* strcpy etc.. */

#ifndef WIN32
#include <sys/ioctl.h>
#include <unistd.h>			/*  */
#include <pwd.h>
#endif

/* MAART: #ifndef __FreeBSD__ */
#if !defined(__FreeBSD__) && !defined(__APPLE__)
#include <malloc.h>
#endif

/* lib includes */
#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "BLI_blenlib.h"
#include "BLI_storage.h"
#include "BLI_storage_types.h"

#include "BLI_util.h"
#include "BLI_linklist.h"

/* functions */
void  BLI_buildpwtable(struct passwd **pwtable);
void  BLI_freepwtable(struct passwd *pwtable);
char *BLI_findpwtable(struct passwd *pwtable, unsigned short user);
 
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
	/* type is gelijk aan stat.st_mode */

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
	return (strcasecmp(entry1->relname,entry2->relname));
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
	struct statfs disk;
	char name[100],*slash;


	strcpy(name,dir);

	if(strlen(name)){
		slash = strrchr(name,'/');
		if (slash) slash[1] = 0;
	} else strcpy(name,"/");

#if defined __FreeBSD__ || defined linux 
	if (statfs(name, &disk)) return(-1);
#endif
#ifdef __BeOS
	return -1;
#endif
#if defined __sgi || defined __sun__

	if (statfs(name, &disk, sizeof(struct statfs), 0)){
		/* printf("diskfree: Couldn't get information about %s.\n",dir); */
		return(-1);
	}
#endif

	return ( ((double) disk.f_bsize) * ((double) disk.f_bfree));
#endif
}

static int hide_dot= 0;

void BLI_hide_dot_files(int set)
{
	if(set) hide_dot= 1;
	else hide_dot= 0;
}

void BLI_builddir(char *dirname, char *relname)
{
	struct dirent *fname;
	struct dirlink *dlink;
	int rellen, newnum = 0, seen_ = 0, seen__ = 0;
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

	if (dir = (DIR *)opendir(".")){
		while ((fname = (struct dirent*) readdir(dir)) != NULL) {
			
			if(hide_dot && fname->d_name[0]=='.' && fname->d_name[1]!='.' && fname->d_name[1]!=0);
			else {
				
				dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
				if (dlink){
					strcpy(buf+rellen,fname->d_name);
	
					dlink->name = strdup(buf);
	
					if (dlink->name[0] == '.') {
						if (dlink->name[1] == 0) seen_ = 1;
						else if (dlink->name[1] == '.') {
							if (dlink->name[2] == 0) seen__ = 1;
						}
					}
					BLI_addhead(dirbase,dlink);
					newnum++;
				}
			}
		}
		
		if (newnum){
#ifndef WIN32		
			if (seen_ == 0) {	/* Cachefs PATCH */
				dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
				strcpy(buf+rellen,"./.");
				dlink->name = strdup(buf);
				BLI_addhead(dirbase,dlink);
				newnum++;
			}
			if (seen__ == 0) {	/* MAC PATCH */
				dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
				strcpy(buf+rellen,"./..");
				dlink->name = strdup(buf);
				BLI_addhead(dirbase,dlink);
				newnum++;
			}
#endif			

			if (files) files=(struct direntry *)realloc(files,(totnum+newnum) * sizeof(struct direntry));
			else files=(struct direntry *)malloc(newnum * sizeof(struct direntry));

			if (files){
				dlink = (struct dirlink *) dirbase->first;
				while(dlink){
					memset(&files[actnum], 0 , sizeof(struct direntry));
					files[actnum].relname = dlink->name;
					stat(dlink->name,&files[actnum].s);
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

#ifndef WIN32
void BLI_buildpwtable(struct passwd **pwtable)
{
	int count=0,slen;
	struct passwd *pw,*pwtab;

	do{
		pw=getpwent();
		if (pw){
			count++;
		}
	}while(pw);
	endpwent();

	pwtab = (struct passwd *)calloc(count+1,sizeof(struct passwd));
	count=0;
	do{
		pw=getpwent();
		if (pw){
			pwtab[count] = *pw;
			slen = strlen(pw->pw_name);
			pwtab[count].pw_name = malloc(slen+1);
			strcpy(pwtab[count].pw_name,pw->pw_name);
			count ++;
		}
	}while(pw);
	pwtab[count].pw_name = 0;
	endpwent();

	*(pwtable) = pwtab;
}

void BLI_freepwtable(struct passwd *pwtable)
{
	int count=0;

	do{
		if (pwtable[count].pw_name) free(pwtable[count].pw_name);
		else break;
		count++;
	}while (1);

	free(pwtable);
}


char *BLI_findpwtable(struct passwd *pwtable, ushort user)
{
	static char string[32];
	
	while (pwtable->pw_name){
		if (pwtable->pw_uid == user){
			return (pwtable->pw_name);
		}
		pwtable++;
	}
	sprintf(string, "%d", user);
	return (string);
}
#endif


void BLI_adddirstrings()
{
	char datum[100];
	char buf[250];
	char size[250];
	static char * types[8] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};
	int num, mode;
	int num1, num2, num3, num4, st_size;
#ifndef WIN32	
	struct passwd *pwtable;
#endif
	struct direntry * file;
	struct tm *tm;
	
#ifndef WIN32
	BLI_buildpwtable(&pwtable);
#endif

	file = &files[0];
	
	for(num=0;num<actnum;num++){
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
		strcpy(files[num].owner,"user");
#else
		strcpy(files[num].owner, BLI_findpwtable(pwtable,files[num].s.st_uid));
#endif

		tm= localtime(&files[num].s.st_mtime);
		strftime(files[num].time, 8, "%H:%M", tm);
		strftime(files[num].date, 16, "%d-%b-%y", tm);
		
		st_size= (int)files[num].s.st_size;
		
		num1= st_size % 1000;
		num2= st_size/1000;
		num2= num2 % 1000;
		num3= st_size/(1000*1000);
		num3= num3 % 1000;
		num4= st_size/(1000*1000*1000);
		num4= num4 % 1000;

		if(num4) sprintf(files[num].size, "%3d %03d %03d %03d", num4, num3, num2, num1);
		else if(num3) sprintf(files[num].size, "%7d %03d %03d", num3, num2, num1);
		else if(num2) sprintf(files[num].size, "%11d %03d", num2, num1);
		else if(num1) sprintf(files[num].size, "%15d", num1);
		else sprintf(files[num].size, "");

		strftime(datum, 32, "%d-%b-%y %R", tm);

		if (st_size < 1000) {
			sprintf(size, "%10d", st_size);
		} else if (st_size < 1000 * 1000) {
			sprintf(size, "%6d %03d", st_size / 1000, st_size % 1000);
		} else if (st_size < 100 * 1000 * 1000) {
			sprintf(size, "%2d %03d %03d", st_size / (1000 * 1000), (st_size / 1000) % 1000, st_size % 1000);
		} else {
			sprintf(size, "> %4.1f M", st_size / (1024.0 * 1024.0));
			sprintf(size, "%10d", st_size);
		}

		sprintf(buf,"%s %s %10s %s", files[num].date, files[num].time, size,
			files[num].relname);

		sprintf(buf,"%s %s %s %7s %s %s %10s %s", file->mode1, file->mode2, file->mode3, files[num].owner, files[num].date, files[num].time, size,
			files[num].relname);

		files[num].string=malloc(strlen(buf)+1);
		if (files[num].string){
			strcpy(files[num].string,buf);
		}

		file++;
	}
#ifndef WIN32
	BLI_freepwtable(pwtable);
#endif
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



int BLI_exist(char *name)
{
	struct stat st;

	if (stat(name,&st)) return(0);
	return(st.st_mode);
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

	buf= malloc(size);
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
		
		free(buf);
	}
	
	fclose(fp);
	
	BLI_linklist_reverse(&lines);
	return lines;
}

void BLI_free_file_lines(LinkNode *lines)
{
	BLI_linklist_free(lines, (void(*)(void*)) MEM_freeN);
}
