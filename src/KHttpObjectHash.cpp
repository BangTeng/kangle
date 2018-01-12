#include <string.h>
#include <string>
#include <stdlib.h>
#include <vector>
#include "KHttpObjectHash.h"
#include "malloc_debug.h"
#include "KCache.h"
void increase_hash_size(KHttpObjectHash* hash, INT64 size, bool mem_flag) {
	assert(hash);
	assert(size>=0);
	assert(hash->size>=0);
	hash->size_lock.Lock();
	if (mem_flag) {
		hash->size += size;
	} else {
		hash->disk_size += size;
	}
	assert(hash->size>=0);
	hash->size_lock.Unlock();
}

void decrease_hash_size(KHttpObjectHash* hash, INT64 size, bool mem_flag) {
	if (!hash) {
		return;
	}
	assert(hash->size>=0);
	hash->size_lock.Lock();
	if (mem_flag) {
		hash->size -= size;
	} else {
		hash->disk_size -= size;
	}
	assert(hash->size>=0);
	hash->size_lock.Unlock();
}

//����obj��content_length;
void set_obj_size(KHttpObject *obj, INT64 content_length) {
	//���û����hash��,��ֱ������
	if (obj->list_state == LIST_IN_NONE) {
		obj->index.content_length = content_length;
		return;
	}
	KHttpObjectHash *hash = cache.getHash(obj->h);
	hash->size_lock.Lock();
	assert(TEST(obj->index.flags,FLAG_IN_MEM));
	//�����µĳ���
	hash->size += content_length;

	//�����ɵĳ���
	hash->size -= obj->index.content_length;
	hash->size_lock.Unlock();

	//����obj�ĳ���
	obj->index.content_length = content_length;
	return;
}

