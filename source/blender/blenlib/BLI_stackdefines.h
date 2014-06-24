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

#ifndef __BLI_STACKDEFINES_H__
#define __BLI_STACKDEFINES_H__

/** \file BLI_stackdefines.h
 *  \ingroup bli
 */

/* simple stack */
#define STACK_DECLARE(stack)   unsigned int _##stack##_index
#define STACK_INIT(stack)      ((void)stack, (void)((_##stack##_index) = 0))
#define STACK_SIZE(stack)      ((void)stack, (_##stack##_index))
#define STACK_PUSH(stack, val)  (void)((stack)[(_##stack##_index)++] = val)
#define STACK_PUSH_RET(stack)  ((void)stack, ((stack)[(_##stack##_index)++]))
#define STACK_PUSH_RET_PTR(stack)  ((void)stack, &((stack)[(_##stack##_index)++]))
#define STACK_POP(stack)            ((_##stack##_index) ?  ((stack)[--(_##stack##_index)]) : NULL)
#define STACK_POP_PTR(stack)        ((_##stack##_index) ? &((stack)[--(_##stack##_index)]) : NULL)
#define STACK_POP_DEFAULT(stack, r) ((_##stack##_index) ?  ((stack)[--(_##stack##_index)]) : r)
/* take care, re-orders */
#define STACK_REMOVE(stack, i) \
	if (--_##stack##_index != i) { \
		stack[i] = stack[_##stack##_index]; \
	} (void)0
#ifdef __GNUC__
#define STACK_SWAP(stack_a, stack_b) { \
	SWAP(typeof(stack_a), stack_a, stack_b); \
	SWAP(unsigned int, _##stack_a##_index, _##stack_b##_index); \
	} (void)0
#else
#define STACK_SWAP(stack_a, stack_b) { \
	SWAP(void *, stack_a, stack_b); \
	SWAP(unsigned int, _##stack_a##_index, _##stack_b##_index); \
	} (void)0
#endif

#endif  /* __BLI_STACKDEFINES_H__ */
