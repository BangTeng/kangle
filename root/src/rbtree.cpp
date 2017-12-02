
#include "rbtree.h"
#include "utils.h"
#include <stdio.h>
#include <assert.h>

#define rb_parent(r)   ((struct rb_node *)((r)->rb_parent_color & ~3))
#define rb_color(r)   ((r)->rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r) rb_color(r)
#define rb_set_red(r)  do { (r)->rb_parent_color &= ~1; } while (0)
#define rb_set_black(r)  do { (r)->rb_parent_color |= 1; } while (0)

static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
	rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}
static inline void rb_set_color(struct rb_node *rb, int color)
{
	rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

#define RB_ROOT	(struct rb_root) { NULL, }
#define	rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root)	((root)->rb_node == NULL)
#define RB_EMPTY_NODE(node)	(rb_parent(node) == node)
#define RB_CLEAR_NODE(node)	(rb_set_parent(node, node))

extern void rb_insert_color(struct rb_node *, struct rb_root *);


typedef void (*rb_augment_f)(struct rb_node *node, void *data);

extern void rb_augment_insert(struct rb_node *node,
							  rb_augment_f func, void *data);
extern struct rb_node *rb_augment_erase_begin(struct rb_node *node);
extern void rb_augment_erase_end(struct rb_node *node,
								 rb_augment_f func, void *data);



/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void rb_replace_node(struct rb_node *victim, struct rb_node *n,struct rb_root *root);








static void __rb_rotate_left(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *right = node->rb_right;
	struct rb_node *parent = rb_parent(node);

	if ((node->rb_right = right->rb_left))
		rb_set_parent(right->rb_left, node);
	right->rb_left = node;

	rb_set_parent(right, parent);

	if (parent)
	{
		if (node == parent->rb_left)
			parent->rb_left = right;
		else
			parent->rb_right = right;
	}
	else
		root->rb_node = right;
	rb_set_parent(node, right);
}

static void __rb_rotate_right(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *left = node->rb_left;
	struct rb_node *parent = rb_parent(node);

	if ((node->rb_left = left->rb_right))
		rb_set_parent(left->rb_right, node);
	left->rb_right = node;

	rb_set_parent(left, parent);

	if (parent)
	{
		if (node == parent->rb_right)
			parent->rb_right = left;
		else
			parent->rb_left = left;
	}
	else
		root->rb_node = left;
	rb_set_parent(node, left);
}

void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *parent, *gparent;

	while ((parent = rb_parent(node)) && rb_is_red(parent))
	{
		gparent = rb_parent(parent);

		if (parent == gparent->rb_left)
		{
			{
				register struct rb_node *uncle = gparent->rb_right;
				if (uncle && rb_is_red(uncle))
				{
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->rb_right == node)
			{
				register struct rb_node *tmp;
				__rb_rotate_left(parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			__rb_rotate_right(gparent, root);
		} else {
			{
				register struct rb_node *uncle = gparent->rb_left;
				if (uncle && rb_is_red(uncle))
				{
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->rb_left == node)
			{
				register struct rb_node *tmp;
				__rb_rotate_right(parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			__rb_rotate_left(gparent, root);
		}
	}

	rb_set_black(root->rb_node);
}

static void __rb_erase_color(struct rb_node *node, struct rb_node *parent,
struct rb_root *root)
{
	struct rb_node *other;

	while ((!node || rb_is_black(node)) && node != root->rb_node)
	{
		if (parent->rb_left == node)
		{
			other = parent->rb_right;
			if (rb_is_red(other))
			{
				rb_set_black(other);
				rb_set_red(parent);
				__rb_rotate_left(parent, root);
				other = parent->rb_right;
			}
			if ((!other->rb_left || rb_is_black(other->rb_left)) &&
				(!other->rb_right || rb_is_black(other->rb_right)))
			{
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			}
			else
			{
				if (!other->rb_right || rb_is_black(other->rb_right))
				{
					rb_set_black(other->rb_left);
					rb_set_red(other);
					__rb_rotate_right(other, root);
					other = parent->rb_right;
				}
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->rb_right);
				__rb_rotate_left(parent, root);
				node = root->rb_node;
				break;
			}
		}
		else
		{
			other = parent->rb_left;
			if (rb_is_red(other))
			{
				rb_set_black(other);
				rb_set_red(parent);
				__rb_rotate_right(parent, root);
				other = parent->rb_left;
			}
			if ((!other->rb_left || rb_is_black(other->rb_left)) &&
				(!other->rb_right || rb_is_black(other->rb_right)))
			{
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			}
			else
			{
				if (!other->rb_left || rb_is_black(other->rb_left))
				{
					rb_set_black(other->rb_right);
					rb_set_red(other);
					__rb_rotate_left(other, root);
					other = parent->rb_left;
				}
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->rb_left);
				__rb_rotate_right(parent, root);
				node = root->rb_node;
				break;
			}
		}
	}
	if (node)
		rb_set_black(node);
}

void rb_erase(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *child, *parent;
	int color;

	if (!node->rb_left)
		child = node->rb_right;
	else if (!node->rb_right)
		child = node->rb_left;
	else
	{
		struct rb_node *old = node, *left;

		node = node->rb_right;
		while ((left = node->rb_left) != NULL)
			node = left;

		if (rb_parent(old)) {
			if (rb_parent(old)->rb_left == old)
				rb_parent(old)->rb_left = node;
			else
				rb_parent(old)->rb_right = node;
		} else
			root->rb_node = node;

		child = node->rb_right;
		parent = rb_parent(node);
		color = rb_color(node);

		if (parent == old) {
			parent = node;
		} else {
			if (child)
				rb_set_parent(child, parent);
			parent->rb_left = child;

			node->rb_right = old->rb_right;
			rb_set_parent(old->rb_right, node);
		}

		node->rb_parent_color = old->rb_parent_color;
		node->rb_left = old->rb_left;
		rb_set_parent(old->rb_left, node);

		goto color;
	}

	parent = rb_parent(node);
	color = rb_color(node);

	if (child)
		rb_set_parent(child, parent);
	if (parent)
	{
		if (parent->rb_left == node)
			parent->rb_left = child;
		else
			parent->rb_right = child;
	}
	else
		root->rb_node = child;

color:
	if (color == RB_BLACK)
		__rb_erase_color(child, parent, root);
}

static void rb_augment_path(struct rb_node *node, rb_augment_f func, void *data)
{
	struct rb_node *parent;

up:
	func(node, data);
	parent = rb_parent(node);
	if (!parent)
		return;

	if (node == parent->rb_left && parent->rb_right)
		func(parent->rb_right, data);
	else if (parent->rb_left)
		func(parent->rb_left, data);

	node = parent;
	goto up;
}

/*
* after inserting @node into the tree, update the tree to account for
* both the new entry and any damage done by rebalance
*/
void rb_augment_insert(struct rb_node *node, rb_augment_f func, void *data)
{
	if (node->rb_left)
		node = node->rb_left;
	else if (node->rb_right)
		node = node->rb_right;

	rb_augment_path(node, func, data);
}

/*
* before removing the node, find the deepest node on the rebalance path
* that will still be there after @node gets removed
*/
struct rb_node *rb_augment_erase_begin(struct rb_node *node)
{
	struct rb_node *deepest;

	if (!node->rb_right && !node->rb_left)
		deepest = rb_parent(node);
	else if (!node->rb_right)
		deepest = node->rb_left;
	else if (!node->rb_left)
		deepest = node->rb_right;
	else {
		deepest = rb_next(node);
		if (deepest->rb_right)
			deepest = deepest->rb_right;
		else if (rb_parent(deepest) != node)
			deepest = rb_parent(deepest);
	}

	return deepest;
}

/*
* after removal, update the tree to account for the removed entry
* and any rebalance damage.
*/
void rb_augment_erase_end(struct rb_node *node, rb_augment_f func, void *data)
{
	if (node)
		rb_augment_path(node, func, data);
}

/*
* This function returns the first node (in sort order) of the tree.
*/
struct rb_node *rb_first(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

struct rb_node *rb_last(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}

struct rb_node *rb_next(const struct rb_node *node)
{
	struct rb_node *parent;

	if (rb_parent(node) == node)
		return NULL;

	/* If we have a right-hand child, go down and then left as far
	as we can. */
	if (node->rb_right) {
		node = node->rb_right; 
		while (node->rb_left)
			node=node->rb_left;
		return (struct rb_node *)node;
	}

	/* No right-hand children.  Everything down and left is
	smaller than us, so any 'next' node must be in the general
	direction of our parent. Go up the tree; any time the
	ancestor is a right-hand child of its parent, keep going
	up. First time it's a left-hand child of its parent, said
	parent is our 'next' node. */
	while ((parent = rb_parent(node)) && node == parent->rb_right)
		node = parent;

	return parent;
}

struct rb_node *rb_prev(const struct rb_node *node)
{
	struct rb_node *parent;

	if (rb_parent(node) == node)
		return NULL;

	/* If we have a left-hand child, go down and then right as far
	as we can. */
	if (node->rb_left) {
		node = node->rb_left; 
		while (node->rb_right)
			node=node->rb_right;
		return (struct rb_node *)node;
	}

	/* No left-hand children. Go up till we find an ancestor which
	is a right-hand child of its parent */
	while ((parent = rb_parent(node)) && node == parent->rb_left)
		node = parent;

	return parent;
}

void rb_replace_node(struct rb_node *victim, struct rb_node *n,
struct rb_root *root)
{
	struct rb_node *parent = rb_parent(victim);

	/* Set the surrounding nodes to point to the replacement */
	if (parent) {
		if (victim == parent->rb_left)
			parent->rb_left = n;
		else
			parent->rb_right = n;
	} else {
		root->rb_node = n;
	}
	if (victim->rb_left)
		rb_set_parent(victim->rb_left, n);
	if (victim->rb_right)
		rb_set_parent(victim->rb_right, n);

	/* Copy the pointers/colour from the victim to the replacement */
	*n = *victim;
}

struct rb_tree *rbtree_create()
{
	struct rb_tree *rb = (struct rb_tree *)xmalloc(sizeof(struct rb_tree));
	rb->root.rb_node = NULL;
	return rb;
}
void rbtree_destroy(struct rb_tree *rb)
{
	xfree(rb);
}
struct rb_node *rbtree_find(struct rb_tree *rb,void *key,comprbt c)
{
	struct rb_node *node = rb->root.rb_node;
	while (node) {	
		int result = c(key,node->data);
		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return node;
	}
	return NULL;
}
void rbtree_remove(struct rb_tree *rb,struct rb_node *node)
{
	rb_erase(node,&rb->root);
	xfree(node);
}
struct rb_node *rbtree_insert(struct rb_tree *rb,void *key,int *new_flag,comprbt c)
{
	struct rb_node **n = &(rb->root.rb_node), *parent = NULL;
	while (*n) {
		int result = c(key,(*n)->data);
		parent = *n;
		if (result < 0) {
			n = &((*n)->rb_left);
		} else if (result > 0) {
			n = &((*n)->rb_right);
		} else {
			*new_flag = 0;
			return (*n);
		}
	}
	struct rb_node *node = (struct rb_node *)xmalloc(sizeof(struct rb_node));
	node->data = key;
	rb_link_node(node, parent, n);
	rb_insert_color(node, &rb->root);
	*new_flag = 1;
	return node;
}
void rbtree_iterator(struct rb_tree *rb,iteratorbt iterator,void *arg)
{
	struct rb_node *node = rb_first(&rb->root);
	while (node) {
		struct rb_node *nn = rb_next(node);
		switch (iterator(node->data,arg)) {
		case iterator_break:
			return;
		case iterator_continue:
			node = nn;
			break;
		case iterator_remove_break:			
			rbtree_remove(rb,node);			
			return;
		case iterator_remove_continue:			
			rbtree_remove(rb,node);
			node = nn;
			break;
		}
	}
}
iterator_ret int_iterator(void *data,void *argv)
{
	int *n = (int *)data;
	printf("n=%d\n",*n);
	xfree(n);
	return iterator_remove_continue;
}
int int_comp(void *k,void *k2)
{
	return *(int *)k - *(int *)k2;
}
void rbtree_test()
{
	struct rb_tree *rb = rbtree_create();
	for (int i=199;i>0;i--) {
		int *j = (int *)xmalloc(sizeof(int));
		*j = i;
		int flag;
		rbtree_insert(rb,j,&flag,int_comp);
		assert(flag ==1);
	}
	int k = 100;
	struct rb_node *n = rbtree_find(rb,&k,int_comp);
	assert(n);
	printf("n->key = %d\n",*(int *)(n->data));
	rbtree_iterator(rb,int_iterator,NULL);
	assert(rb->root.rb_node==NULL);
}
