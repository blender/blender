/*
**
*/

typedef struct VNetOutQueue VNetOutQueue;

extern VNetOutQueue * v_noq_create_network_queue(void);
extern void	v_noq_destroy_network_queue(VNetOutQueue *queue);
extern void	v_noq_send_buf(VNetOutQueue *queue, VCMDBufHead *buf);
extern void	v_noq_send_ack_nak_buf(VNetOutQueue *queue, VCMDBufHead *buf);

extern void	v_noq_send_ack(VNetOutQueue *queue, unsigned int id);
extern void	v_noq_send_nak(VNetOutQueue *queue, unsigned int id);

extern boolean v_noq_send_queue(VNetOutQueue *queue, void *address);

extern unsigned int v_noq_get_next_out_packet_id(VNetOutQueue *queue);
