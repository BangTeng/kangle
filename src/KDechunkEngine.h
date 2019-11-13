#ifndef KDECHUNKENGINE_H
#define KDECHUNKENGINE_H
#include "global.h"
#include <stdlib.h>
#include <string.h>
#include "kmalloc.h"

enum dechunk_status
{
	dechunk_success,
	dechunk_continue,
	dechunk_end,
	dechunk_failed
};
class KDechunkEngine {
public:
	KDechunkEngine() {
		chunk_size = 0;
		work_len = 0;
		work = NULL;
	}
	~KDechunkEngine() {
		if (work) {
			free(work);
		}
	}
	//返回dechunk_continue表示还需要读数据，
	dechunk_status dechunk(const char **buf, int &buf_len, const char **piece, int &piece_length);
	bool is_failed() {
		return work_len < -4;
	}
	bool is_success() {
		return work_len == -4;
	}
	bool is_end() {
		return work_len <= -4;
	}
private:
	int chunk_size ;
	int work_len ;
	char *work;
};
#endif
