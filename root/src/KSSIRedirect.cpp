/*
 * KSSIRedirect.cpp
 *
 *  Created on: 2010-8-2
 *      Author: keengo
 */
#include "KSSIFetchObject.h"
#include "KSSIRedirect.h"
#include "malloc_debug.h"
KSSIRedirect ssi;
KSSIRedirect::KSSIRedirect() {

}
KSSIRedirect::~KSSIRedirect() {
}
KFetchObject *KSSIRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file) {
	return new KSSIFetchObject();
}
