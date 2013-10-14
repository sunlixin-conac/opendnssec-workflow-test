

#ifndef KEYGEN_H
#define	KEYGEN_H

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

typedef enum _flag
{
     rsa=0,
     ecc=1
}flag;

typedef struct _KeyGen
{
    int keysize;
    flag type;

}KeyGen;
 
void keygeneration(KeyGen *);

char * prikey;
//void pem2der(RSA *);
//RSA der2pem(const unsigned char *, int );
char *der2base64(const unsigned char *, int);
char *base64toder( char *);



#endif	

