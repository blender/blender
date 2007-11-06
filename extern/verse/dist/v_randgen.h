/*
 * Random number generator API. A way to improve over rand().
*/

#if !defined V_RANDGEN_H
#define V_RANDGEN_H

typedef struct VRandGen	VRandGen;

extern VRandGen *	v_randgen_new(void);
extern void		v_randgen_get(VRandGen *gen, void *bytes, size_t num);
extern void		v_randgen_destroy(VRandGen *gen);

#endif		/* V_RANDGEN_H */
