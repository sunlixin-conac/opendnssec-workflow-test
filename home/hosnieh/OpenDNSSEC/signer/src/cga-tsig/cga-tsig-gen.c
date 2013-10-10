#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cga-tsig-gen.h"
#include "signgen.h"
int gen(unsigned char * data)
{
    
    
    
}

char * OtherDataGen( unsigned  alg, unsigned int type, char * iptag, char * update)
{
 unsigned char * alldata ;
    strcat((char*)alg,(char*)type);
    strcat((char*)type,(char*)iptag);
    
     //char * data = rsasigngen()
}

