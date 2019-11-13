#ifndef MSOCKET_RBTREE_H
#define MSOCKET_RBTREE_H
#include "kfeature.h"
#include <stdlib.h>
KBEGIN_DECLS
typedef int(*comprbt) (void *key1, void *key2);
int rbtree_int_cmp(void *key1, void *key2);

typedef enum
{
	iterator_continue,
	iterator_remove_continue,
	iterator_break,
	iterator_remove_break
} iterator_ret;
typedef iterator_ret (*iteratorbt) (void *data, void *argv);

struct krb_node
{
	uintptr_t  rb_parent_color;
#define	RB_RED		0
#define	RB_BLACK	1
	struct krb_node *rb_right;
	struct krb_node *rb_left;
	void *data;
};
struct krb_root
{
	struct krb_node *rb_node;
};
struct krb_tree
{
	struct krb_root root;
};
typedef struct krb_node *(*new_node)();
typedef void(*free_node)(struct krb_node *node);

/* Find logical next and previous nodes in a tree */
extern struct krb_node *rb_next(const struct krb_node *);
extern struct krb_node *rb_prev(const struct krb_node *);
extern struct krb_node *rb_first(const struct krb_root *);
extern struct krb_node *rb_last(const struct krb_root *);

struct krb_tree *rbtree_create();
void rbtree_destroy(struct krb_tree *rb);
struct krb_node *rbtree_find(struct krb_tree *rb, void *key, comprbt c);
struct krb_node *rbtree_find2(struct krb_tree *rb, void *key, comprbt c, int *result);
struct krb_node *rbtree_find_cover(struct krb_tree *rb, void *key, comprbt c);
struct krb_node *rbtree_insert(struct krb_tree *rb, void *key, int *new_flag, comprbt c);
struct krb_node *rbtree_insert2(struct krb_tree *rb, void *key, int *new_flag, comprbt c, new_node nn);
void rbtree_iterator(struct krb_tree *rb, iteratorbt iterator, void *arg);
void rbtree_remove(struct krb_tree *rb, struct krb_node *node);
void rbtree_remove2(struct krb_tree *rb, struct krb_node *node, free_node fn);
void rbtree_test();

/////////////////////////////////////////////////////////////
INLINE void rb_link_node(struct krb_node * node, struct krb_node * parent, struct krb_node ** rb_link)
{
	node->rb_parent_color = (uintptr_t)parent;
	node->rb_left = node->rb_right = NULL;
	*rb_link = node;
}
void rb_insert_color(struct krb_node *node, struct krb_root *root);
void rb_erase(struct krb_node *node, struct krb_root *root);
KEND_DECLS
#endif
