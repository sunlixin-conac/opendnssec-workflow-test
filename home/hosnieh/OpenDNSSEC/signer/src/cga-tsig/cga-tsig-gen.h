

#ifndef CGATSIGGEN_H
#define	CGATSIGGEN_H


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



#endif	

