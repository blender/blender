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
 * Contributor(s): Michel Selten,
 *                 Willian P. Germano,
 *                 Chris Keith,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BPY_extern.h
 *  \ingroup python
 */



#ifndef __BPY_EXTERN_H__
#define __BPY_EXTERN_H__

struct PathResolvedRNA;
struct Text; /* defined in DNA_text_types.h */
struct ID; /* DNA_ID.h */
struct Object; /* DNA_object_types.h */
struct ChannelDriver; /* DNA_anim_types.h */
struct ListBase; /* DNA_listBase.h */
struct bConstraint; /* DNA_constraint_types.h */
struct bPythonConstraint; /* DNA_constraint_types.h */
struct bConstraintOb; /* DNA_constraint_types.h */
struct bConstraintTarget; /* DNA_constraint_types.h*/
struct bContext;
struct bContextDataResult;
struct ReportList;

#ifdef __cplusplus
extern "C" {
#endif

void BPY_pyconstraint_exec(struct bPythonConstraint *con, struct bConstraintOb *cob, struct ListBase *targets);
//	void BPY_pyconstraint_settings(void *arg1, void *arg2);
void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct);
void BPY_pyconstraint_update(struct Object *owner, struct bConstraint *con);
int BPY_is_pyconstraint(struct Text *text);
//	void BPY_free_pyconstraint_links(struct Text *text);

void BPY_python_start(int argc, const char **argv);
void BPY_python_end(void);
void BPY_python_reset(struct bContext *C);


/* global interpreter lock */

typedef void *BPy_ThreadStatePtr;

BPy_ThreadStatePtr BPY_thread_save(void);
void BPY_thread_restore(BPy_ThreadStatePtr tstate);

/* our own wrappers to Py_BEGIN_ALLOW_THREADS/Py_END_ALLOW_THREADS */
#define BPy_BEGIN_ALLOW_THREADS { BPy_ThreadStatePtr _bpy_saved_tstate = BPY_thread_save(); (void)0
#define BPy_END_ALLOW_THREADS BPY_thread_restore(_bpy_saved_tstate); } (void)0

bool	BPY_execute_filepath(struct bContext *C, const char *filepath, struct ReportList *reports);
bool	BPY_execute_text(struct bContext *C, struct Text *text, struct ReportList *reports, const bool do_jump);

bool	BPY_execute_string_as_number(struct bContext *C, const char *imports[], const char *expr, const bool verbose, double *r_value);
bool	BPY_execute_string_as_intptr(struct bContext *C, const char *imports[], const char *expr, const bool verbose, intptr_t *r_value);
bool	BPY_execute_string_as_string(struct bContext *C, const char *imports[], const char *expr, const bool verbose, char **r_value);

bool	BPY_execute_string_ex(struct bContext *C, const char *expr, bool use_eval);
bool	BPY_execute_string(struct bContext *C, const char *expr);

void	BPY_text_free_code(struct Text *text);
void	BPY_modules_update(struct bContext *C); // XXX - annoying, need this for pointers that get out of date
void	BPY_modules_load_user(struct bContext *C);

void	BPY_app_handlers_reset(const short do_all);

void	BPY_driver_reset(void);
float	BPY_driver_exec(struct PathResolvedRNA *anim_rna, struct ChannelDriver *driver, const float evaltime);

void	BPY_DECREF(void *pyob_ptr);	/* Py_DECREF() */
void	BPY_DECREF_RNA_INVALIDATE(void *pyob_ptr);
int		BPY_context_member_get(struct bContext *C, const char *member, struct bContextDataResult *result);
void	BPY_context_set(struct bContext *C);
void	BPY_context_update(struct bContext *C);

void	BPY_id_release(struct ID *id);

bool	BPY_string_is_keyword(const char *str);

/* I18n for addons */
#ifdef WITH_INTERNATIONAL
const char *BPY_app_translations_py_pgettext(const char *msgctxt, const char *msgid);
#endif

#ifdef __cplusplus
}				/* extern "C" */
#endif

#endif  /* __BPY_EXTERN_H__ */
