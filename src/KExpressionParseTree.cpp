/*
 * KNode.cpp
 *
 *  Created on: 2010-8-2
 *      Author: keengo
 */
static const char *ended_chars = "=<>()&|!";
#include "KExpressionParseTree.h"
#include "KReg.h"
#include "utils.h"
#include "malloc_debug.h"
using namespace std;
KStringNode::KStringNode(KSSIContext *context, char *value) :
	KNode(Node_string) {
	this->context = context;
	this->value = value;
	resolved = NULL;
}
KStringNode::~KStringNode() {
	if (value) {
		xfree(value);
	}
	if (resolved) {
		xfree(resolved);
	}
}
bool KStringNode::parse() {
	if (resolved) {
		return true;
	}

	resolved = context->parseString(value);
	if (resolved == NULL) {
		return false;
	}
	int len = strlen(resolved);
	if (*resolved == '/' && resolved[len - 1] == '/') {
		isReg = true;
		resolved[len - 1] = '\0';
	} else {
		isReg = false;
	}
	return true;
}
KStringNode *KStringNode::convert(KNode *b) {
	if (b->getType() != Node_string) {
		return NULL;
	}
	if (!parse()) {
		return NULL;
	}
	KStringNode *node = (KStringNode *) b;
	if (!node->parse()) {
		return NULL;
	}
	return node;
}
bool KStringNode::equal(KNode *b) {
	KStringNode *node = convert(b);
	if (node == NULL) {
		return false;
	}
	if (node->isReg) {
		KReg reg;
		if (!reg.setModel(node->resolved + 1, 0)) {
			return false;
		}
		if (reg.match(resolved, strlen(resolved), 0) > 0) {
			return true;
		}
		return false;
	}
	return strcmp(resolved, node->resolved) == 0;

}
bool KStringNode::great(KNode *b) {
	KStringNode *node = convert(b);
	if (node == NULL) {
		return false;
	}
	return strcmp(resolved, node->resolved) > 0;
}
bool KStringNode::less(KNode *b) {
	KStringNode *node = convert(b);
	if (node == NULL) {
		return false;
	}
	return strcmp(resolved, node->resolved) < 0;
}
void KStringNode::concat(const char *val) {

}
const char *KStringNode::getValue() {
	return NULL;
}
bool KExpressionParseTree::setStringToken() {
	if (cur_con == NULL) {
		cur_con = new KCondition;
		if (cur_gcon == NULL) {
			cur_gcon = new KGroupCondition;
		}
		cur_gcon->addCondition(cur_con);
	}
	if (cur_con->left == NULL) {
		//assert(cur_con==NULL);
		cur_con->left = new KStringNode(context, value);
		value = NULL;
	} else {
		if (cur_con->op == NULL) {
			return false;
		}
		cur_con->right = new KStringNode(context, value);
		value = NULL;
	}
	return true;
}
ExpResult KExpressionParseTree::evaluate(char *expr) {
	hot = expr;
	ExpToken token;
	for (;;) {
		token = getNextToken();
		/*		if (token == Token_string && value) {
		 printf("string=[%s]\n", value);
		 }
		 */
		switch (token) {
		case Token_end:
			goto done;
		case Token_string:
			if (!setStringToken()) {
				//todo:Óï·¨´íÎó
				return Exp_failed;
			}
			break;
		case Token_op_equal:
		case Token_op_greate:
		case Token_op_less:
		case Token_op_greate_equal:
		case Token_op_less_equal:
			newOperatorToken(token);
			break;
		case Token_logic_and:
		case Token_logic_or:

			if (!newLogicToken(token)) {
				return Exp_failed;
			}
			break;
		case Token_logic_not:
			if (cur_con == NULL) {
				//todo:Óï·¨´íÎó
				return Exp_failed;
			}
			cur_con->revert();
			break;
		case Token_left_bracket:
			if (!newGCondition()) {
				return Exp_failed;
			}
			break;
		case Token_right_bracket:
			if (!endGCondition()) {
				//todo:Óï·¨´íÎó
				return Exp_failed;
			}
			break;
		}
	}
	done: if (conStack.size() > 0) {
		//À¨ºÅÃ»ÓÐ¶ÔÆë
		return Exp_failed;
	}
	if (cur_gcon == NULL) {
		return Exp_failed;
	}
	if (cur_gcon->evaluate()) {
		return Exp_true;
	}
	return Exp_false;
}
bool KExpressionParseTree::newLogicToken(ExpToken token) {
	if (cur_con == NULL) {
		//todo:Óï·¨´íÎó
		return false;
	}
	switch (token) {
	case Token_logic_and:
		cur_con->logic_after = Logic_and;
		break;
	case Token_logic_or:
		cur_con->logic_after = Logic_or;
		break;
	default:
		return false;
	}

	if (!endCondition()) {
		//TODO:Óï·¨´íÎó
		return false;
	}
	return true;
}

bool KExpressionParseTree::newOperatorToken(ExpToken token) {
	if (cur_con == NULL) {
		//todo:Óï·¨´íÎó
		return false;
	}
	if (cur_con->op != NULL) {
		//todo:Óï·¨´íÎó
		return false;
	}
	switch (token) {
	case Token_op_equal:
		cur_con->op = new KEqualOperator();
		break;
	case Token_op_greate:
		cur_con->op = new KGreatOperator();
		break;
	case Token_op_greate_equal:
		cur_con->op = new KLessOperator();
		cur_con->revert();
		break;
	case Token_op_less:
		cur_con->op = new KLessOperator();
		break;
	case Token_op_less_equal:
		cur_con->op = new KGreatOperator();
		cur_con->revert();
		break;
	default:
		return false;
	}
	return true;
}
ExpToken KExpressionParseTree::getNextToken() {
	if (hot == NULL) {
		return Token_end;
	}
	while (*hot && isspace((unsigned char) *hot))
		hot++;
	//printf("hot=[%s]\n", hot);
	switch (*hot) {
	case '\0':
		return Token_end;
	case '=':
		hot++;
		return Token_op_equal;
	case '>':
		if (hot[1] == '=') {
			hot += 2;
			return Token_op_greate_equal;
		}
		hot++;
		return Token_op_greate;
	case '<':
		if (hot[1] == '=') {
			hot += 2;
			return Token_op_less_equal;
		}
		hot++;
		return Token_op_less;
	case '(':
		hot++;
		return Token_left_bracket;
	case ')':
		hot++;
		return Token_right_bracket;
	case '&':
		if (hot[1] == '&') {
			hot += 2;
			return Token_logic_and;
		}
		hot++;
		return Token_logic_and;
	case '|':
		if (hot[1] == '|') {
			hot += 2;
			return Token_logic_or;
		}
		hot++;
		return Token_logic_or;
	case '!':
		hot++;
		return Token_logic_not;
	}
	if (value) {
		xfree(value);
		value = NULL;
	}
	char *tokenValue = getString(hot, &hot, ended_chars);
	if (hot && tokenValue) {
		int len = hot - tokenValue;
		assert(len>=0);
		if (len > 0) {
			value = (char *) xmalloc(len+1);
			memcpy(value, tokenValue, len);
			value[len] = '\0';
		}
	} else {
		if (tokenValue) {
			value = strdup(tokenValue);
		}
	}
	//printf("string value=[%s],next_string=[%s]\n", value, hot);
	return Token_string;
}
bool KExpressionParseTree::endCondition() {
	if (cur_con == NULL || cur_con->op == NULL) {
		return false;
	}
	if (cur_gcon == NULL) {
		return false;
	}
	cur_con = NULL;
	return true;
}
bool KExpressionParseTree::newGCondition() {
	if (cur_gcon) {
		conStack.push_front(cur_gcon);
	}
	cur_gcon = new KGroupCondition();
	return true;
}
bool KExpressionParseTree::endGCondition() {
	if (cur_gcon == NULL) {
		return false;
	}
	std::list<KGroupCondition *>::iterator it;
	it = conStack.begin();
	if (it == conStack.end()) {
		return false;
	}
	(*it)->addCondition(cur_gcon);
	cur_gcon = (*it);
	conStack.pop_front();
	return true;
}
KExpressionParseTree::KExpressionParseTree() {
	cur_gcon = NULL;
	cur_con = NULL;
	value = NULL;
	hot = NULL;
	context = NULL;
}
KExpressionParseTree::~KExpressionParseTree() {
	if (value) {
		xfree(value);
	}
	if (cur_gcon) {
		delete cur_gcon;
	}
	list<KGroupCondition *>::iterator it;
	for (it = conStack.begin(); it != conStack.end(); it++) {
		delete (*it);
	}
}
KGroupCondition::~KGroupCondition() {
	std::list<KCondition *>::iterator it;
	for (it = cons.begin(); it != cons.end(); it++) {
		delete (*it);
	}
}
KCondition::~KCondition() {
	if (op) {
		delete op;
	}
	if (left) {
		delete left;
	}
	if (right) {
		delete right;
	}
}
bool KGroupCondition::evaluate() {
	std::list<KCondition *>::iterator it;
	flag = true;
	logic_after = Logic_and;
	for (it = cons.begin(); it != cons.end(); it++) {
		if (logic_after == Logic_or) {
			if (flag == true) {
				return true;
			}
		} else if (logic_after == Logic_and) {
			if (flag == false) {
				return false;
			}
		}
		flag = (*it)->evaluate();
		logic_after = (*it)->logic_after;
	}
	return flag;
}
