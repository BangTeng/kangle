#include "KDsoExtend.h"
#include "KConfig.h"
#include "extern.h"
#include "KDynamicString.h"
#include "klog.h"
#include "kselector_manager.h"
static int get_selector_index()
{
	kselector *selector = kgl_get_tls_selector();
	if (selector == NULL) {
		return -1;
	}
	return selector->sid;
}
KDsoExtend::KDsoExtend(const char *name)
{
	this->cur_config_ext = cur_config_ext;
	this->name = xstrdup(name);
	filename = NULL;
	orign_filename = NULL;
	memset(&version, 0, sizeof(version));
}
KDsoExtend::~KDsoExtend()
{
	xfree(name);
	if (filename) {
		xfree(filename);
	}
	if (orign_filename) {
		xfree(orign_filename);
	}
}
bool KDsoExtend::RegisterUpstream(kgl_upstream *us)
{
	std::map<const char *, KDsoRedirect *, lessp>::iterator it;
	it = upstream.find(us->name);
	if (it != upstream.end()) {
		return false;
	}
	upstream.insert(std::pair<const char *, KDsoRedirect *>(us->name,new KDsoRedirect(name,us)));
	return true;
}
KRedirect *KDsoExtend::RefsRedirect(std::string &name)
{
	std::map<const char *, KDsoRedirect *, lessp>::iterator it;
	it = upstream.find(name.c_str());
	if (it == upstream.end()) {
		return NULL;
	}
	KRedirect *rd = (*it).second;
	rd->addRef();
	return rd;
}
void KDsoExtend::ListUpstream(std::stringstream &s)
{
	std::map<const char *, KDsoRedirect *, lessp>::iterator it;
	for (it = upstream.begin(); it != upstream.end(); it++) {
		s << (*it).first << " ";
	}
}
void KDsoExtend::ListTarget(std::vector<std::string> &target)
{
	std::map<const char *, KDsoRedirect *, lessp>::iterator it;
	for (it = upstream.begin(); it != upstream.end(); it++) {
		std::stringstream s;
		s << "dso:" << this->name << ":" << (*it).first;
		target.push_back(s.str());
	}
}
bool KDsoExtend::load(const char *filename)
{
	orign_filename = strdup(filename);
	KDynamicString ds;
	this->filename = ds.parseString(filename);
	if (!dso.load(this->filename)) {
		klog(KLOG_ERR, "cann't load dso extend [%s]\n", name);
		return false;
	}

	kgl_dso_init = (kgl_dso_init_f)dso.findFunction("kgl_dso_init");
	if (kgl_dso_init == NULL) {
		klog(KLOG_ERR, "cann't load dso extend [%s] kgl_dso_init function cann't find\n", name);
		return false;
	}
	kgl_dso_finit = (kgl_dso_finit_f)dso.findFunction("kgl_dso_finit");
	memset(&version, 0, sizeof(version));
	version.global_support_function = global_support_function;
	version.get_variable = global_get_variable;
	version.get_selector_count = get_selector_count;
	version.get_selector_index = get_selector_index;
	version.ctx = this;
	if (!kgl_dso_init(&version)) {
		klog(KLOG_ERR, "cann't load dso extend [%s] kgl_dso_init return false\n", name);
		return false;
	}
	return true;
}
