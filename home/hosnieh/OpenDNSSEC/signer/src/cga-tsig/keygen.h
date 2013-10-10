

#ifndef KEYGEN_H
#define	KEYGEN_H


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
 
void keygeneration(KeyGen *key);

char * prikey;


#endif	

