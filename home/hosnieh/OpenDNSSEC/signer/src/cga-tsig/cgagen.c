#include "cgagen.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "public_variable.h"
#include "readfile.h"
#include "keygen.h"


// if the data is an array of bytes
unsigned char * sha1GenU(unsigned char * data)
{

size_t length = sizeof(data);

unsigned char hash[SHA_DIGEST_LENGTH];
SHA1(data, length, hash);

return hash;

}
// if the data is in string format
unsigned char * sha1Gen(char * data)
{

size_t length = sizeof(data);

unsigned char hash[SHA_DIGEST_LENGTH];
SHA1(data, length, hash);

return hash;

}
unsigned char * randGen(int number_of_bytes)
{
    unsigned char rn[number_of_bytes];
    int i;
    srand ( time(NULL) );
  for (i = 1; i <= number_of_bytes; i++) {
    rn[i] = rand()%254 + 1;
    printf("%d\n", rn[i]);
  }
    return rn;
    
}

unsigned char * cgaGen(unsigned char * ra, short keygen, char * keypath)
{
    switch(keygen)
    {
        case 0:
            char * prikey=ReadFile(keypath);
            
            break;
        case 1:
            
            break;
     
        
    }
            
            
   
    
}



