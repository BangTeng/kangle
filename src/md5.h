#ifndef md5_h_slkdjfs097fd9s7df98sflsjdfjlsdf2o3u234
#define md5_h_slkdjfs097fd9s7df98sflsjdfjlsdf2o3u234
typedef unsigned uint32;
typedef struct {
        uint32 state[4];                            /* state (ABCD) */
        uint32 count[2];
        unsigned char buffer[64];       /* input buffer */
} KMD5_CTX;
typedef KMD5_CTX KMD5Context;
void KMD5Init(KMD5_CTX *);
void KMD5Update(KMD5_CTX *, const unsigned char *, unsigned int);
void KMD5Final(unsigned char[16], KMD5_CTX *);
void KMD5 (const char *buf,char *digest,int len=0);
void KMD5BIN (const char *buf,char *result,int len=0);
void make_digest(char *md5str, unsigned char *digest);
#endif
