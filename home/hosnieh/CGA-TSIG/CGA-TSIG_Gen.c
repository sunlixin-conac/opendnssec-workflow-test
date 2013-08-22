
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SignatureGen.h"
int gen(unsigned char * data)
{
    
    
    
}

char * OtherDataGen( unsigned  alg, unsigned int type, char * iptag, char * update)
{
 unsigned char * alldata ;
    strcat(alg,type);
    strcat(type,iptag);
    
     char * data = rsasigngen()
}

