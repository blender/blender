/*
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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __CREATOR_INTERN_H__
#define __CREATOR_INTERN_H__

/** \file creator/creator_intern.h
 *  \ingroup creator
 *
 * Functionality for main() initialization.
 */

struct bArgs;
struct bContext;

#ifndef WITH_PYTHON_MODULE

/* creator_args.c */
void main_args_setup(struct bContext *C, struct bArgs *ba, SYS_SystemHandle *syshandle);
void main_args_setup_post(struct bContext *C, struct bArgs *ba);


/* creator_signals.c */
void main_signal_setup(void);
void main_signal_setup_background(void);
void main_signal_setup_fpe(void);

#endif  /* WITH_PYTHON_MODULE */


/* Shared data for argument handlers to store state in */
struct ApplicationState {
	struct {
		bool use_crash_handler;
		bool use_abort_handler;
	} signal;

	/* we may want to set different exit codes for other kinds of errors */
	struct {
		unsigned char python;
	} exit_code_on_error;

};
extern struct ApplicationState app_state;  /* creator.c */

/* for the callbacks: */
#ifndef WITH_PYTHON_MODULE
#define BLEND_VERSION_FMT         "Blender %d.%02d (sub %d)"
#define BLEND_VERSION_ARG         BLENDER_VERSION / 100, BLENDER_VERSION % 100, BLENDER_SUBVERSION
/* pass directly to printf */
#define BLEND_VERSION_STRING_FMT  BLEND_VERSION_FMT "\n", BLEND_VERSION_ARG
#endif

#ifdef WITH_BUILDINFO_HEADER
#  define BUILD_DATE
#endif

/* from buildinfo.c */
#ifdef BUILD_DATE
 extern char build_date[];
extern char build_time[];
extern char build_hash[];
extern unsigned long build_commit_timestamp;

/* TODO(sergey): ideally size need to be in sync with buildinfo.c */
extern char build_commit_date[16];
extern char build_commit_time[16];

extern char build_branch[];
extern char build_platform[];
extern char build_type[];
extern char build_cflags[];
extern char build_cxxflags[];
extern char build_linkflags[];
extern char build_system[];
#endif

#endif  /* __CREATOR_INTERN_H__ */
