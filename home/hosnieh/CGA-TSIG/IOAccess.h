#ifndef IOACCESS_H
#define	IOACCESS_H

#ifdef	__cplusplus
extern "C" {
#endif

unsigned char* ReadFile(char* );
void WriteFile(char* , unsigned char * );
char * ReadConfigFile();


#ifdef	__cplusplus
}
#endif

#endif	/* IOACCESS_H */

