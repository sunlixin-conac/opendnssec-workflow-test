#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>



char * ObtainIP(char * networkinterface,char * IPString)
{
    
    //test
    //networkinterface="eth0";
            
struct ifaddrs *ifaddr, *ifa;
   int family, s;
   char host[NI_MAXHOST];

   if (getifaddrs(&ifaddr) == -1) {
        perror("no interface available!");
        exit(EXIT_FAILURE);
   }

   for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                               host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                if (s != 0) {
                        printf("getnameinfo() failed: %s\n", gai_strerror(s));
                        exit(EXIT_FAILURE);
                }
                
               // printf("<Interface>: %s \t <Address> %s\n", ifa->ifa_name, host);
                if(strcmp(ifa->ifa_name,networkinterface)==0)
                {
                    IPString=&host;
                    return host;
                }
        }
   }

   return 0;
    
}

char* ObtainIP6(char * networkinterface, char * IPString)
{ 
    unsigned char IPByte[16];
    
    //test
    //networkinterface="eth0";
            
struct ifaddrs *ifaddr, *ifa;
   int family, s;
   char host[NI_MAXHOST];
   struct sockaddr_in6 sa;
   
   if (getifaddrs(&ifaddr) == -1) {
        perror("no interface available!");
        exit(EXIT_FAILURE);
   }

   for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        family = ifa->ifa_addr->sa_family;
     //  printf("%s", ifa->ifa_addr);
        if (family == AF_INET6) {
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6),
                                               host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                 printf("%s", ifa->ifa_addr);
                if (s != 0) {
                        printf("getnameinfo() failed: %s\n", gai_strerror(s));
                        exit(EXIT_FAILURE);
                }
                
                 
                
               // printf("<Interface>: %s \t <Address> %s\n", ifa->ifa_name, host);
                if(strcmp(ifa->ifa_name,networkinterface)==0)
                {
                      if (inet_pton(family, host, &sa.sin6_addr)==1)
                  {
                
                          strcpy(IPByte,&sa.sin6_addr);
                      //IPByte=&sa.sin6_addr;
                          /*
                          
                      
                    printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[0]);
                    printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[1]);
                      printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[2]);
                        printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[3]);
                          printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[4]);
                            printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[5]);
                              printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[6]);
                                printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[7]);
                                  printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[8]);
                                    printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[9]);
                                      printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[10]);
                                         printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[11]);
                                         printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[12]);
                                         printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[13]);
                                      printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[14]);
                                         printf("%d\n",sa.sin6_addr.__in6_u.__u6_addr8[15]);
                                           
                      
                         printf("%d\n",IPByte[15]);*/
                      
                      }
                 
                    freeifaddrs(ifaddr);
                    IPString=&host;
                    return IPByte;
                   
                  // return the IPv6 striing
                 //   return host;
                  
                }
        }
   }
freeifaddrs(ifaddr);
   return 0;
    
}

