#include "openssl/rsa.h"
#include <openssl/pem.h>
#include "IOAccess.h"
#include "public_variable.h"

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
 
extern void keygeneration(KeyGen *key)
{
  
    switch(key->type)
    {
        case 0://ecc
   
            break;
      
        case 1://rsa
        { //  error message without this : a label can only be a part of statement
         int exp = 3;  
    
            RSA* rsa = RSA_generate_key(key->keysize,exp , 0, 0);
            
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
       
}






