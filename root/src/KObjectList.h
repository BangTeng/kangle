#ifndef KOBJECTLIST_H
#define KOBJECTLIST_H
#include "global.h"
#include "KMutex.h"
#include "KHttpObject.h"
class KVirtualHost;
class KObjectList
{
public:
	KObjectList();
	int clean_cache(KReg *reg,int flag);
	void add(KHttpObject *obj);
	void dead(KHttpObject *obj);
	void remove(KHttpObject *obj);
	void move(INT64 size,bool swapout);
	void dead_count(int &count);
	void dead_all_obj();
	void swap_all_obj();
	void dump_refs_obj(std::stringstream &s);
	int save_disk_index(KFile *fp);
	unsigned char list_state;
	void getSize(INT64 &csize,INT64 &cdsiz);
	KHttpObject *getHead()
	{
		return l_head;
	}
#ifdef MALLOCDEBUG
	void free_all_cache();
#endif
private:
	void checkList()
	{
		if (l_head==NULL) {
			assert(l_end==NULL);
			return;
		}
		assert(l_end);
		assert(l_head->lprev == NULL);
		assert(l_end->lnext ==NULL);
	}
	KHttpObject *l_head;
	KHttpObject *l_end;	
	int cache_model;
};
#endif
