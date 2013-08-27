#include <stdio.h>
#include <stdlib.h>
#include <cga-tsig/cga-tsig-lib.h>


/*
 * 
 */
int main(int argc, char** argv) {
    
   // readxml("../../ConfigFile.xml");
    ////testing source IP taken
   // char a[6]= "eth0\0";
   // char * ip = ObtainIP6(a);
   // printf("\n%d",ip[3]);
    char a[100]="ConfigFile.xml\0";
    readxml(a);

    return (EXIT_SUCCESS);
}

