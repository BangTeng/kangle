#ifndef KDECHUNK_CONTEXT_H
#define KDECHUNK_CONTEXT_H
#include "kselector.h"
#include "KDechunkEngine.h"
class KHttpSink;
class KDechunkContext
{
public:
	KDechunkContext()
	{
		memset(this, 0, sizeof(*this));
	}
	kev_result Read(KHttpSink *sink,void *arg, result_callback result, buffer_callback buffer);
	kev_result ReadResult(KHttpSink *sink, int got);
	void SetChunkRawRead()
	{
		raw_read = true;
	}
private:
	kev_result ParseBuffer(KHttpSink *sink);
	kev_result ReadChunk(KHttpSink *sink);
	bool raw_read;
	const char *hot;
	int hot_len;
	const char *chunk;
	int chunk_left;
	KDechunkEngine eng;
	void *arg;
	result_callback result;
	buffer_callback buffer;
};
#endif
