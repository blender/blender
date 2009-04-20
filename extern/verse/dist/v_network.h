/*
**
*/

#if !defined V_NETWORK_H
#define	V_NETWORK_H

#define VERSE_STD_CONNECT_PORT 4950

typedef struct{
	unsigned int ip;
	unsigned short port;
}VNetworkAddress;

extern void		v_n_set_port(unsigned short port);
extern unsigned int	v_n_wait_for_incoming(unsigned int microseconds);
extern boolean	v_n_set_network_address(VNetworkAddress *address, const char *host_name);
extern int		v_n_send_data(VNetworkAddress *address, const char *data, size_t length);
extern int		v_n_receive_data(VNetworkAddress *address, char *data, size_t length);
extern void		v_n_get_address_string(const VNetworkAddress *address, char *string);

extern void		v_n_get_current_time(unsigned int *seconds, unsigned int *fractions);

#endif		/* V_NETWORK_H */
