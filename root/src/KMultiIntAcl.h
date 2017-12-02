#ifndef KMULTIINTACL_H
#define KMULTIINTACL_H
#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "utils.h"
#include "rbtree.h"

class KMultiIntAcl: public KAcl {
public:
	KMultiIntAcl() {
		split = '|';
		root.rb_node = NULL;
	}
	virtual ~KMultiIntAcl() {
		freeMap();
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input name=v size=40 value='";
		KMultiIntAcl *acl = (KMultiIntAcl *) (model);
		if (acl) {
			s << acl->getValList();
		}
		s << "'>";		
		s << "split:<input name=split max=1 size=1 value='";
		if (acl == NULL) {
			s << "|";
		} else {
			s << acl->split;
		}
		s << "'>";
		return s.str();
	}
	std::string getValList() {
		std::stringstream s;
		rb_node *node;
		bool isFirst = true;
		for (node=rb_first(&root); node!=NULL; node=rb_next(node)) {
			if (!isFirst) {
				s << split;
			}
			isFirst = false;
			s << *((int *)node->data);
		}
		return s.str();
	}
	std::string getDisplay() {
		std::stringstream s;
		s << getValList();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
			throw (KHtmlSupportException) {
		freeMap();
		if (attibute["split"].size() > 0) {
			split = attibute["split"][0];
		} else {
			split = '|';
		}
		if (attibute["v"].size() > 0) {
			explode(attibute["v"].c_str());
		}
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {
		if (len > 0) {
			freeMap();
			explode(character);
		}
		return true;
	}

	void buildXML(std::stringstream &s) {
		s << " split='" << split << "'>" << getValList();
	}
protected:
	bool match(int item) {
		struct rb_node **n = &(root.rb_node);
		int *data;
		while (*n) {
			data = (int *)((*n)->data);
			int result = item - *data;
			if (result < 0)
				n = &((*n)->rb_left);
			else if (result > 0)
				n = &((*n)->rb_right);
			else
				return true;			
		}
		return false;
	}
private:
	void insert(int *item)
	{
		struct rb_node **n = &(root.rb_node), *parent = NULL;
		int *data;
		while (*n) {
			data = (int *)((*n)->data);
			int result = *item - *data;
			parent = *n;
			if (result < 0)
				n = &((*n)->rb_left);
			else if (result > 0)
				n = &((*n)->rb_right);
			else{
				delete item;
				return;
			}
		}
		rb_node *node = new rb_node;
		node->data = item;
		rb_link_node(node, parent, n);
		rb_insert_color(node, &root);
	}
	void explode(const char *str)
	{
		char *buf = strdup(str);
		char *hot = buf;
		while(hot){
			char *p = strchr(hot,split);
			if (p) {
				*p = '\0';
			}
			if(*hot){
				int *item = new int;
				*item = atoi(hot);
				insert(item);
			}
			if (p) {
				hot = p+1;
			} else {
				hot = NULL;
			}			
		}
		free(buf);
	}	
	void freeMap() {
		for(;;){
			rb_node *node = rb_first(&root);
			if(node==NULL){
				break;
			}
			assert(node->data);
			int *item = (int *)node->data;
			delete item;
			rb_erase(node,&root);
			delete node;
		}
	}
	char split;
protected:
	rb_root root;
};
#endif
