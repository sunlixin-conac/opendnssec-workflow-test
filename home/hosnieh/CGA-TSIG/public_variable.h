#ifndef PUBLIC_VARIABLE_H
#define	PUBLIC_VARIABLE_H

    char * pubkey;
    char * prikey;
    char * filecontent[5];
    int keysize;
    
    /* RSA key exponential*/
    
    
    // Return IPv4 address
   char * ObtainIP(char *);
   /* Return IPv4 address in array of bytes
   How to call this function
    char a[6]= "eth0\0";
    char * ip = ObtainIP6(a);
    */
   char * ObtainIP6(char *);


#endif	/* PUBLIC_VARIABLE_H */

