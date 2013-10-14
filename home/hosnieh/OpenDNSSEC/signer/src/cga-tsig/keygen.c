#include "readfile.h"
#include "keygen.h"


 
void keygeneration(KeyGen *key)
{
  
    switch(key->type)
    {
        case 0://ecc
   
                break;
      
        case 1://rsa
        { //  error message without this : a label can only be a part of statement
                int exp = 3;
               
                RSA* rsa = RSA_generate_key(key->keysize,exp , 0, 0);
                 // Check whether or not the key is good. If it is not good regenerate a new key
                //RSA_check_key(rsa)==1 : key is good
                while (!RSA_check_key(rsa))
                {
                rsa = RSA_generate_key(key->keysize,exp , 0, 0);
                }
                
         /* To get the C-string PEM form: */
                BIO *prikey = BIO_new(BIO_s_mem());
                BIO *pubkey = BIO_new(BIO_s_mem());
                
                char *pem_key;
                PEM_write_bio_RSAPrivateKey(prikey, rsa, NULL, NULL, 0, NULL, NULL);
                PEM_write_bio_RSAPublicKey(pubkey, rsa);

                int pri_len = BIO_pending(prikey);
                int pub_len = BIO_pending(pubkey);
                
                char *pri_key = calloc(pri_len + 1,1);
                char *pub_key = calloc(pub_len + 1,1);
                //pem_key = malloc(keylen+1, 1); /* Null-terminate */
               // BIO_read(bio, pem_key, keylen);
                BIO_read(prikey, pri_key, pri_len);
                BIO_read(pubkey, pub_key, pub_len);

                //printf("%s", pem_key);
                pri_key[pri_len] = '\0';
                pub_key[pub_len] = '\0';
                
                WriteFile("key/prikey.pem",pri_key);
                WriteFile("key/pubkey.pem",pub_key);
                prikey = pem_key;
                pem2der(rsa);
                
                BIO_free_all(prikey);
                BIO_free_all(pubkey);
                RSA_free(rsa);
                free(pri_key);
                 free(pub_key);
                break;
        }
    }
       
}

void pem2der(RSA *key)
{
    //RSA * d2i_RSAPublicKey(RSA **a, unsigned char **pp, long length);
//	int i2d_RSAPublicKey(RSA *a, unsigned char **pp);
 
  
  int len = i2d_RSA_PUBKEY( key, NULL);
  unsigned char* dercode = OPENSSL_malloc(len);
  i2d_RSAPublicKey(key, &dercode);
  char * b64=der2base64(dercode,len);
  WriteFile("key/pubkey.der",b64);
  
  OPENSSL_free(dercode);

  return b64;
       
}
RSA * der2pem(const unsigned char *der_key, int pubkey_size)
{
  RSA *decoded_key = d2i_RSAPublicKey(NULL,&der_key,pubkey_size);
  return decoded_key;
       
}

char *der2base64(const unsigned char *dercode, int length)
{
    BIO *_bio, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    _bio = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, _bio);
    BIO_write(b64, dercode, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *buff = (char *)malloc(bptr->length);
    memcpy(buff, bptr->data, bptr->length-1);
    buff[bptr->length-1] = 0;

    BIO_free_all(b64);

    return buff;
}

int DecodeLength(const char* in_b64) 
{ //Calculates the length of a decoded base64 string
     int len = strlen(in_b64);
     int padding = 0;
 
      if (in_b64[len-1] == '=' && in_b64[len-2] == '=') //last two chars are =
         padding = 2;
       else if (in_b64[len-1] == '=') //last char is =
         padding = 1;
 
     return (int)len*0.75 - padding;
}
 
char *base64toder( char *in_b64)
{
 BIO *bio,*b64, *bio_out;
 int inlen;

 b64 = BIO_new(BIO_f_base64());
 bio = BIO_new_fp(stdin, BIO_NOCLOSE);
 bio_out = BIO_new_fp(stdout, BIO_NOCLOSE);
 bio = BIO_push(b64, bio);
 while((inlen = BIO_read(bio, in_b64, DecodeLength)) > 0)
        BIO_write(bio_out, in_b64, inlen);

 BIO_free_all(bio);


    return bio_out;
}


