#include "openssl/rsa.h"
#include <openssl/pem.h>
#include "IOAccess.h"
#include "public_variable.h"

enum flag 
{
    rsa=0,
    ecc=1
};

struct KeyGen
{
    int keysize;
    flag type;

};
 
extern void keygeneration(struct KeyGen *key)
{
    
    switch (key->type)
    {
        case flag.ecc:
            
            
            
            break;
            
        case flag.rsa:
                 const int exp = 3;
            RSA *rsa = RSA_generate_key(key->keysize,exp , 0, 0);
            
         /* To get the C-string PEM form: */
           BIO *bio = BIO_new(BIO_s_mem());
           int keylen;
char *pem_key;
PEM_write_bio_RSAPrivateKey(bio, rsa, NULL, NULL, 0, NULL, NULL);

keylen = BIO_pending(bio);
pem_key = calloc(keylen+1, 1); /* Null-terminate */
BIO_read(bio, pem_key, keylen);

//printf("%s", pem_key);
WriteFile("key/prikey.pem",pem_key);
prikey = pem_key;

BIO_free_all(bio);
RSA_free(rsa);
free(pem_key);
         break;
    }
       
}






