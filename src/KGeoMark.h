#ifndef KGEOMARK_H
#define KGEOMARK_H
#include "KMark.h"
#include "rbtree.h"
#include "KIpMap.h"
#include "utils.h"
#include "KLineFile.h"
iterator_ret free_geo_env(void *data, void *argv);
class KGeoMark : public KMark
{
public:
	KGeoMark()
	{
		env = NULL;
		last_modified = 0;
		pool = NULL;
		current_lable = NULL;
		total_item = 0;
	}
	~KGeoMark()
	{
		clean_env();
	}
	bool supportRuntime()
	{
		return true;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType, int &jumpType) {
		char *lable = (char *)im.find(rq->getClientIp());
		if (lable == NULL) {
			return false;
		}
		rq->parser.insertHeader(this->name.c_str(), this->name.size(), lable, strlen(lable));
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << this->name << " " << total_item;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attr)
		throw(KHtmlSupportException) {
		this->file = attr["file"];
		this->name = attr["name"];

		std::string file;
		if (isAbsolutePath(this->file.c_str())) {
			file = this->file;
		} else {
			file = conf.path + this->file;
		}
		time_t last_modified = kfile_last_modified(file.c_str());
		if (last_modified == this->last_modified) {
			return;
		}
		this->last_modified = last_modified;
		im.clear();
		clean_env();
		env = rbtree_create();
		pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
		KStreamFile lf;
		if (!lf.open(file.c_str())) {
			return;
		}
		for (;;) {
			char *line = lf.read();
			if (line == NULL) {
				break;
			}
			if (*line=='*') {
				current_lable = pool_strdup(line + 1);
				total_item++;
				continue;
			}
			if (current_lable == NULL) {
				continue;
			}
			im.add_addr(line, current_lable);
		}		
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {		
		return true;
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KGeoMark *m = (KGeoMark *)model;
		s << "name:<input name='name' value='";
		if (m) {
			s << m->name;
		}
		s << "'>";
		s << "file:<input name='file' value='";
		if (m) {
			s << m->file;
		}
		s << "'>";

		return s.str();
	}
	KMark *newInstance() {
		return new KGeoMark;
	}
	const char *getName() {
		return "geo";
	}
	void buildXML(std::stringstream &s) {
		s << " file='" << this->file << "'";
		s << " name='" << this->name << "'";
		s << ">";
	}
private:
	char *pool_strdup(const char *str)
	{
		int len = strlen(str);
		char *data = (char *)kgl_pnalloc(pool, len+1);
		memcpy(data, str, len);
		data[len] = '\0';
		return data;
	}
	void clean_env()
	{
		if (env) {
			rbtree_iterator(env, free_geo_env,NULL);
			rbtree_destroy(env);
			env = NULL;
		}
		if (pool) {
			kgl_destroy_pool(pool);
			pool = NULL;
		}
		current_lable = NULL;
		total_item = 0;
	}
	std::string file;
	std::string name;
	KIpMap im;
	rb_tree *env;
	kgl_pool_t *pool;
	time_t last_modified;
	char *current_lable;
	int total_item;
};
#endif

