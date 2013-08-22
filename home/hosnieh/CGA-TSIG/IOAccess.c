/*Author: Hosnieh Rafiee */
#include <stdio.h>
#include <stdlib.h>
#include "public_variable.h"
 
unsigned char* ReadFile(char* filepath)
{
    
  char* ch;
   FILE *fp;
 
   fp = fopen(filepath,"r"); // read mode
 
   if( fp == NULL )
   {
      perror("Error while opening the file.\n");
      exit(EXIT_FAILURE);
   }
   int i=0;
    while( ( ch[i] = fgetc(fp) ) != EOF )
        i++;
  
   i++;
   ch[i]='\0';//end of file
   fclose(fp);
   return ch;
}

void WriteFile(char* filepath, unsigned char * content)
{
    
  char* ch;
   FILE *fp;
 
   fp = fopen(filepath,"w"); // read mode
 
   if( fp == NULL )
   {
      perror("Error while opening the file.\n");
      exit(EXIT_FAILURE);
   }
   fprintf(fp, "%s", content);
 
   fclose(fp);
   return;
}
char * ReadConfigFile()
{
    /* file format
     * line 1 : relative path to public private key 
     * line 2: algorithm
     * line 3: keysize
     * line 4 iidalgorithm
     * line 5 parameters path  where one can find the parameters for that specific algorithm
     
     */
    char * content=ReadFile("ConfigFile");
    int i=0;
    int j=0;
    int l=0;
    short k=0;
    while(content[l]!='\0')
    {
        
        if(k==1)
        {
        filecontent[j][i]=content[l];  
        if(content[l]=='\n')
        {
            k=0;
            j++;
        }
        i++;
        }
        if(content[l]=='=')
            k=1;
        l++;
         
        
        
    }
 
    
    
}