#include "KUrl.h"
void free_url(KUrl *url) {
	url->destroy();
}
char *get_url_key(KUrlKey *uk, int *len)
{
	KStringBuf s;
	uk->url->GetUrl(s);
	if (uk->vary) {
		kassert(uk->vary->key && uk->vary->val);
		s.WSTR("\n");
		s << uk->vary->key;
		s.WSTR("\n");
		s << uk->vary->val;
	}
	if (len) {
		*len = s.getSize();
	}
	return s.stealString();
}
void update_url_vary_key(KUrlKey *uk, const char *key)
{
	if (key == NULL) {
		kassert(uk->vary);
		xfree(uk->vary->key);
		uk->vary->key = NULL;
		return;
	}
	if (uk->vary == NULL) {
		uk->vary = new KVary;
	}
	if (uk->vary->key) {
		xfree(uk->vary->key);
	}
	uk->vary->key = strdup(key);
	return;	
}
