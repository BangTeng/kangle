#ifndef KSSLSERIALACL_H
#define KSSLSERIALACL_H
#include "KMultiAcl.h"
#include "KSocket.h"
#include "ssl_utils.h"
#ifdef KSOCKET_SSL
class KSSLSerialAcl: public KMultiAcl {
public:
	KSSLSerialAcl(){

	}
	KAcl *newInstance() {
		return new KSSLSerialAcl();
	}
	const char *getName() {
		return "ssl_serial";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (TEST(rq->workModel,WORK_MODEL_SSL)) {
			KSSLSocket *sslSocket = static_cast<KSSLSocket *> (rq->c->socket);
			char *serial = ssl_var_lookup(sslSocket->getSSL(),
					"CERT_SERIALNUMBER");
			if (serial == NULL) {
				return false;
			}
			bool result = KMultiAcl::match(serial);
			OPENSSL_free(serial);
			return result;
		}
		return false;
	}
};
#endif
#endif
