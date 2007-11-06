/*
 * Miscellaneous utility routines for generic use throughout the code.
*/

/* Safe, buffer size limited, string copy. */
extern char *	v_strlcpy(char *dst, const char *src, size_t size);

typedef struct {
	uint32 seconds;
	uint32 fractions;
} VUtilTimer;

extern void	v_timer_start(VUtilTimer *timer);
extern void	v_timer_advance(VUtilTimer *timer, double seconds);
extern double	v_timer_elapsed(const VUtilTimer *timer);
extern void	v_timer_print(const VUtilTimer *timer);

extern int	v_quat32_valid(const VNQuat32 *q);
extern int	v_quat64_valid(const VNQuat64 *q);
extern VNQuat32*v_quat32_from_quat64(VNQuat32 *dst, const VNQuat64 *src);
extern VNQuat64*v_quat64_from_quat32(VNQuat64 *dst, const VNQuat32 *src);
