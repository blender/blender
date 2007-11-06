
typedef struct{
	void	*oldest;
	void	*newest;
	uint32	packet_id;
	uint32	seconds, fractions;		/* Current time. */
	uint32	acc_seconds, acc_fractions;	/* Accumulated time. */
}VNetInQueue;

typedef struct{
	void	*newer;
	void	*older;
	char	data[1500];
	size_t	size;
}VNetInPacked;

extern void		v_niq_clear(VNetInQueue *queue);
extern void		v_niq_timer_update(VNetInQueue *queue);

extern VNetInPacked *	v_niq_get(VNetInQueue *queue, size_t *length);
extern void		v_niq_release(VNetInQueue *queue, VNetInPacked *p);
extern char *		v_niq_store(VNetInQueue *queue, size_t length, unsigned int packet_id);
unsigned int		v_niq_free(VNetInQueue *queue);
extern uint32		v_niq_time_out(VNetInQueue *queue);
