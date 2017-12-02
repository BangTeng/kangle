#ifndef	KANGLE_RBTREE_H
#define	KANGLE_RBTREE_H
#include <stdlib.h>
typedef int (*comprbt) (void *key1, void *key2);
enum iterator_ret
{
	iterator_continue,
	iterator_remove_continue,
	iterator_break,
	iterator_remove_break
};
typedef iterator_ret (*iteratorbt) (void *data,void *argv);

struct rb_node
{
	unsigned long  rb_parent_color;
#define	RB_RED		0
#define	RB_BLACK	1
	struct rb_node *rb_right;
	struct rb_node *rb_left;
	void *data;
};
struct rb_root
{
	struct rb_node *rb_node;
};
struct rb_tree
{
	struct rb_root root;
};
/* Find logical next and previous nodes in a tree */
extern struct rb_node *rb_next(const struct rb_node *);
extern struct rb_node *rb_prev(const struct rb_node *);
extern struct rb_node *rb_first(const struct rb_root *);
extern struct rb_node *rb_last(const struct rb_root *);

struct rb_tree *rbtree_create();
void rbtree_destroy(struct rb_tree *rb);
struct rb_node *rbtree_find(struct rb_tree *rb,void *key,comprbt c);
struct rb_node *rbtree_insert(struct rb_tree *rb,void *key,int *new_flag,comprbt c);
void rbtree_iterator(struct rb_tree *rb,iteratorbt iterator,void *arg);
void rbtree_remove(struct rb_tree *rb,struct rb_node *node);
void rbtree_test();

/////////////////////////////////////////////////////////////
inline void rb_link_node(struct rb_node * node, struct rb_node * parent,	struct rb_node ** rb_link)
{
	node->rb_parent_color = (unsigned long )parent;
	node->rb_left = node->rb_right = NULL;
	*rb_link = node;
}
void rb_insert_color(struct rb_node *node, struct rb_root *root);
void rb_erase(struct rb_node *, struct rb_root *);
#endif
