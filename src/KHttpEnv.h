#ifndef KANGLE_KHTTPENV_H
#define KANGLE_KHTTPENV_H
#include "KHttpHeader.h"
class KHttpEnv {
public:
	virtual bool add(const char *name, hlen_t name_len, const char *val, hlen_t val_len) = 0;
	inline bool add(know_http_header name, const char *val, hlen_t val_len)
	{
		return true;
	}
	inline bool add(KHttpHeader *header)
	{
		return add(header->attr, header->attr_len, header->val, header->val_len);
	}
	inline bool add(kgl_str_t *name, kgl_str_t *val)
	{
		return add(name->data, hlen_t(name->len), val->data, hlen_t(val->len));
	}
	inline bool add(kgl_str_t *name, const char *val, hlen_t val_len)
	{
		return add(name->data, hlen_t(name->len), val, hlen_t(val_len));
	}
	inline bool add(const char *name, hlen_t name_len, int val)
	{
		char buf[16];
		int len = snprintf(buf, sizeof(buf) - 1, "%d", val);
		return add(name, name_len, buf, len);
	}
};
class KStreamHttpEnv : public KHttpEnv {
public:
	KStreamHttpEnv(KWStream *s)
	{
		this->s = s;
	}
	bool add(const char *name, hlen_t name_len, const char *val, hlen_t val_len)
	{
		s->write_all(name, name_len);
		s->write_all(": ", 2);
		s->write_all(val, val_len);
		s->write_all("\r\n", 2);
		return true;
	}
private:
	KWStream *s;
};
#endif

