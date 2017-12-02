/*
 * KNode.h
 *
 *  Created on: 2010-8-2
 *      Author: keengo
 */

#ifndef KNODE_H_
#define KNODE_H_
#include <list>
#include <string>
#include <string.h>
#include "KSSIContext.h"
#include "global.h"
enum NodeType {
	Node_string
};
enum ExpToken {
	Token_end,
	Token_string,
	Token_op_equal,
	Token_op_greate,
	Token_op_less,
	Token_op_greate_equal,
	Token_op_less_equal,
	Token_logic_and,
	Token_logic_or,
	Token_logic_not,
	Token_left_bracket,
	Token_right_bracket
};
enum Logic_t {
	Logic_and, Logic_or
};

class KNode {
public:
	KNode(NodeType type) {
		this->type = type;
	}
	virtual ~KNode() {

	}
	virtual bool equal(KNode *b) {
		return false;
	}
	virtual bool great(KNode *b) {
		return false;
	}
	virtual bool less(KNode *b) {
		return false;
	}
	NodeType getType() {
		return type;
	}
private:
	NodeType type;
};
class KOperator {
public:
	KOperator() {
	}
	virtual ~KOperator() {

	}
	virtual bool op(KNode *left, KNode *right) = 0;
};
class KCondition {
public:
	KCondition() {
		op = NULL;
		flag = false;
		left = NULL;
		right = NULL;
	}
	virtual ~KCondition();
	virtual bool evaluate() {
		if (op == NULL) {
			return false;
		}
		return op->op(left, right) != flag;
	}
	KOperator *op;
	Logic_t logic_after;
	void revert() {
		flag = !flag;
	}
	bool flag;
	KNode *left;
	KNode *right;
};
class KGroupCondition: public KCondition {
public:
	KGroupCondition() {

	}
	~KGroupCondition();
	void addCondition(KCondition *con) {
		cons.push_back(con);
	}
	bool evaluate();
private:
	std::list<KCondition *> cons;
};
enum ExpResult {
	Exp_false, Exp_true, Exp_failed
};
class KExpressionParseTree {
public:
	KExpressionParseTree();
	~KExpressionParseTree();
	ExpResult evaluate(char *expr);
	void setContext(KSSIContext *context) {
		this->context = context;
	}
private:
	ExpToken getNextToken();
	bool setStringToken();
	bool endCondition();
	bool newGCondition();
	bool endGCondition();
	KSSIContext *context;
	bool newLogicToken(ExpToken token);
	bool newOperatorToken(ExpToken token);
	KCondition *cur_con;
	KGroupCondition *cur_gcon;
	std::list<KGroupCondition *> conStack;
	char *hot;
	char *value;
};
class KStringNode: public KNode {
public:
	KStringNode(KSSIContext *context, char *value);
	~KStringNode();

	void concat(const char *val);
	const char *getValue();
	KStringNode *convert(KNode *b);
	bool parse();
	bool equal(KNode *b);
	bool great(KNode *b);
	bool less(KNode *b);
private:
	char *value;
	char *resolved;
	KSSIContext *context;
	bool isReg;
};
class KEqualOperator: public KOperator {
public:
	bool op(KNode *left, KNode *right) {
		//(KNode *left, KNode *right) {
		return left->equal(right);
	}
};
class KGreatOperator: public KOperator {
public:
	bool op(KNode *left, KNode *right) {
		return left->great(right);
	}
};
class KLessOperator: public KOperator {
public:
	bool op(KNode *left, KNode *right) {
		return left->less(right);
	}
};

#endif /* KNODE_H_ */
