#ifndef MSOCKET_LIST_H
#define MSOCKET_LIST_H
#ifndef _WIN32
#include <stddef.h>
#endif
typedef struct kgl_list_s kgl_list;
struct kgl_list_s {
	kgl_list  *prev;
	kgl_list  *next;
};
#define klist_init(list) do {\
        (list)->next = (list);\
        (list)->prev = (list);\
} while(0)
#define klist_empty(list) ((list)->next == list)
//x will insert before list
#define klist_insert(list,  x)   do {\
    (x)->prev = (list)->prev;\
    (x)->prev->next = x; \
    (x)->next = (list); \
    (list)->prev = x;\
} while (0)

#define klist_append(list,  new_link) klist_insert(list,new_link)
#define klist_insert_tail klist_insert

#define klist_remove(link) do {\
        (link)->prev->next = (link)->next;\
        (link)->next->prev = (link)->prev;\
} while(0)
#define klist_head(list) (list)->next;
#define klist_end(list)  (list)->prev;
#define klist_foreach(pos, list)                  \
        for (pos = (list)->next;                  \
		pos != (list);                      \
		pos = pos->next)
#define klist_rforeach(pos, list)                 \
        for (pos = (list)->prev;                  \
		pos != (list);                      \
		pos = pos->prev)
#define kgl_list_data(list, type, sub_type) (type *) ((unsigned char *)list - offsetof(type, sub_type))
#endif
