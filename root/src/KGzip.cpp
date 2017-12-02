/*
 * KGzip.cpp
 *
 *  Created on: 2010-5-4
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#include <string.h>
#include "do_config.h"
#include "KGzip.h"
#include "KHttpTransfer.h"
#include "malloc_debug.h"

KGzipCompress::KGzipCompress(bool use_deflate,KWStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
{
	fast = false;
	isSuccess = false;
	this->use_deflate = use_deflate;
	memset(&strm,0,sizeof(strm));
	if (deflateInit2(&strm, conf.gzip_level,
		Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
	return;
	out = (char *) malloc(CHUNK);
	if (out == NULL) {
		klog(KLOG_ERR, "no memory to alloc\n");
		return;
	}
	if (!use_deflate) {
		sprintf(out, "%c%c%c%c%c%c%c%c%c%c", 0x1f, 0x8b, Z_DEFLATED, 0 /*flags*/,
				0, 0, 0, 0 /*time*/, 0 /*xflags*/, 0);
		used = 10;
		crc = crc32(0L, Z_NULL, 0);
	}
	isSuccess = true;
}
KGzipCompress::~KGzipCompress()
{
	if (out) {
		xfree(out);
	}
	deflateEnd(&strm);
}
StreamState KGzipCompress::write_all(const char *str,int len)
{
	strm.avail_in = len;
	strm.next_in = (unsigned char *) str;
	if (!use_deflate) {
		crc = crc32(crc, (unsigned char *) str, len);
	}
	if(compress(Z_NO_FLUSH)){
		return STREAM_WRITE_SUCCESS;
	}
	return STREAM_WRITE_FAILED;
}
StreamState KGzipCompress::write_end()
{
	if (strm.total_in<= 0) {
		return STREAM_WRITE_FAILED;
	}
	if (!compress(Z_FINISH)) {
		return STREAM_WRITE_FAILED;
	}
	if (used + 8 > CHUNK || !fast) {
		char *buf = (char *) xmalloc(used+8);
		memcpy(buf, out, used);
		xfree(out);
		out = buf;
	}
	if (!use_deflate) {
		int n;
		for (n = 0; n < 4; n++) {
			out[used + n] = (crc & 0xff);
			crc >>= 8;
		}
		used += 4;
		unsigned totalLen2 = strm.total_in;
		for (n = 0; n < 4; n++) {
			out[used + n] = (totalLen2 & 0xff);
			totalLen2 >>= 8;
		}
		used += 4;
	}
	StreamState result = st->write_direct(out, used);
	out = NULL;
	if(result!=STREAM_WRITE_SUCCESS){
		return result;
	}
	return KHttpStream::write_end();
}
StreamState KGzipCompress::compress(int flush_flag)
{
	if(!isSuccess){
		return STREAM_WRITE_FAILED;
	}
	do {
		if (out == NULL) {
			out = (char *) xmalloc(CHUNK);
			if (out == NULL) {
				klog(KLOG_ERR, "no memory to alloc\n");
				return STREAM_WRITE_FAILED;
			}
			used = 0;
		}
		strm.avail_out = CHUNK - used;
		strm.next_out = (unsigned char *) (out + used);
		int ret = deflate(&strm, flush_flag);//(flush_flag ? Z_FINISH : Z_NO_FLUSH)); /* no bad return value */
		//		int ret = deflate(&strm, (flush ? Z_FINISH : Z_SYNC_FLUSH)); /* no bad return value */

		assert(ret != Z_STREAM_ERROR); /* state not clobbered */
		if (ret == Z_STREAM_ERROR) {
			return STREAM_WRITE_FAILED;
		}
		unsigned have = CHUNK - used - strm.avail_out;
		used += have;
		if (used >= CHUNK) {
			StreamState result =st->write_direct(out, used);
			//printf("this=%p,up=%p\n",this,st);
			out = NULL;
			if(result != STREAM_WRITE_SUCCESS){
				return result;
			}
		}
	} while (strm.avail_out == 0);
	return STREAM_WRITE_SUCCESS;
}
KGzipDecompress::KGzipDecompress(bool use_deflate,KWStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
{
	isSuccess = false;
	memset(&strm,0,sizeof(strm));
	out = NULL;
	used = 0;
	fast = true;
	this->use_deflate = use_deflate;
	if (use_deflate) {
		in_skip = 0;
	} else {
		in_skip = 10;
	}
	if (inflateInit2(&strm,-MAX_WBITS) != Z_OK) {
		return;
	}
	isSuccess = true;
	return;
}
StreamState KGzipDecompress::decompress(int flush_flag)
{
	int ret = 0;
	do {
		if (out == NULL) {
			out = (char *) xmalloc(CHUNK);
			if (out == NULL) {
				klog(KLOG_ERR, "no memory to alloc\n");
				return STREAM_WRITE_FAILED;
			}
			used = 0;
		}
		strm.avail_out = CHUNK - used;
		strm.next_out = (unsigned char *) (out + used);
		ret = inflate(&strm, flush_flag); /* no bad return value */
		if (ret < 0) {
			return STREAM_WRITE_FAILED;
		}
		int have = CHUNK - used - strm.avail_out;
		used += have;
		if (used >= CHUNK) {
			StreamState result = st->write_direct(out,used);
			out = NULL;
			if(result != STREAM_WRITE_SUCCESS){
				return result;
			}
		}
	} while (strm.avail_out==0);
	return STREAM_WRITE_SUCCESS;
}
StreamState KGzipDecompress::write_end()
{
	if (STREAM_WRITE_FAILED==decompress(Z_FINISH)) {
		return STREAM_WRITE_FAILED;
	}
	if (used > 0 && out) {
		if(!fast && CHUNK-used > 64){
			char *new_out = (char *)xmalloc(used);
			memcpy(new_out,out,used);
			xfree(out);
			out = new_out;
		}
		StreamState result = st->write_direct(out,used);
		out = NULL;
		if(result != STREAM_WRITE_SUCCESS){
			return result;
		}
	}
	return KHttpStream::write_end();
}
StreamState KGzipDecompress::write_all(const char *str,int len)
{
	int skip_len = MIN(in_skip,len);
	in_skip -= skip_len;
	strm.avail_in = len - skip_len;
	strm.next_in = (unsigned char *) str + skip_len;
	if (strm.avail_in>0) {
		return decompress(Z_NO_FLUSH);
	}
	return STREAM_WRITE_SUCCESS;
}
KGzipDecompress::~KGzipDecompress()
{
	if (out) {
		xfree(out);
	}
	inflateEnd(&strm);
}
