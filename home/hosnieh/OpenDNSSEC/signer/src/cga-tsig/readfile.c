/*Author: Hosnieh Rafiee 16-09-2013 */
#include <stdio.h>
#include <stdlib.h>
#include "readfile.h" 
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

    
    
