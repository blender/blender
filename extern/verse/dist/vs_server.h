/*
**
*/

#include <stdlib.h>

extern void			vs_init_connection_storage(void);
extern void			vs_add_new_connection(VSession session, const char *name, const char *pass, VNodeID node_id);
extern void			vs_remove_connection(void);
extern void			vs_set_next_session(void);

typedef void VSSubscriptionList;

extern VSSubscriptionList * vs_create_subscription_list(void);
extern void			vs_destroy_subscription_list(VSSubscriptionList *list);
extern int			vs_add_new_subscriptor(VSSubscriptionList *list);
extern void			vs_remove_subscriptor(VSSubscriptionList *list);
extern unsigned int		vs_get_subscript_count(const VSSubscriptionList *list);
extern void			vs_set_subscript_session(VSSubscriptionList *list, unsigned int session);
extern void			vs_reset_subscript_session(void);
extern uint32		vs_get_avatar(void);
extern VSession		vs_get_session(void);
extern const char *		vs_get_user_name(void);
extern const char *		vs_get_user_pass(void);


typedef struct {
	VNodeID			id;
	VNodeType		type;
	VNodeID			owner;
	char			*name;
	void			*tag_groups;
	uint16			group_count;
	VSSubscriptionList	*subscribers;
} VSNodeHead;

extern void			vs_init_node_storage(void);
extern uint32		vs_add_new_node(VSNodeHead *node, VNodeType type);
extern VSNodeHead *	vs_get_node(unsigned int node_id, VNodeType type);
extern VSNodeHead *	vs_get_node_head(unsigned int node_id);

extern void			create_node_head(VSNodeHead *node, const char *name, unsigned int owner);
extern void			destroy_node_head(VSNodeHead *node);
extern void			vs_send_node_head(VSNodeHead *node);

extern void			vs_h_callback_init(void);	/* "Head", not an actual node type. */
extern void			vs_o_callback_init(void);
extern void			vs_g_callback_init(void);
extern void			vs_m_callback_init(void);
extern void			vs_b_callback_init(void);
extern void			vs_t_callback_init(void);
extern void			vs_c_callback_init(void);
extern void			vs_a_callback_init(void);
extern void			init_callback_node_storage(void);

extern void		vs_master_set_enabled(boolean enabled);
extern void		vs_master_set_address(const char *address);
extern const char *	vs_master_get_address(void);
extern void		vs_master_set_desc(const char *desc);
extern void		vs_master_set_tags(const char *tags);
extern void		vs_master_update(void);
extern void		vs_master_handle_describe(const char *address, const char *message);
