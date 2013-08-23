#include <stdio.h>
#include "openssl/rsa.h"
#include <openssl/pem.h>
#include "public_variable.h"
int rsasigngen(char * data, char * digest){

        char *data1 = NULL;
        int data_len;
        unsigned int sig_len;
        int err = -1;

        OpenSSL_add_all_digests();
        FILE *fd;
        EVP_PKEY *priv_key = EVP_PKEY_new();
        RSA *privkey = NULL;
           printf( "we are here..\n");

        if ((fd = fopen(filecontent[0], "r")) == NULL){
            printf("error reading file\n");
            exit(0);
        }

        privkey = RSA_new();
        if (!PEM_read_PrivateKey(fd, &privkey, NULL, NULL))
        {
            fprintf(stderr, "Error loading RSA Private Key File.\n");
            return 2;
        }

        fclose(fd);

        if (!EVP_PKEY_assign_RSA (priv_key, privkey))
        {
            fprintf(stderr, "EVP_PKEY_assign_RSA: failed.\n");
            return 3;
        }

        if (!priv_key) {
            printf("no private key\n");
        }
        EVP_PKEY_set1_RSA(privkey, priv_key);


        EVP_MD_CTX *ctx = EVP_MD_CTX_create();

        const EVP_MD *md = EVP_get_digestbyname("SHA1");

        if (!md) {
            fprintf(stderr, "Error creating message digest");
            fprintf(stderr, " object, unknown name?\n");
            ERR_print_errors_fp(stderr);
            exit(1);
        }



        if (!EVP_SignInit(ctx, md))
            {
                fprintf(stderr, "EVP_SignInit: failed.\n");
                EVP_PKEY_free(priv_key);
                return 3;
            }
        printf( "now to sign update..\n");
       // data1 = readFile();
        data_len = strlen(data);
        printf("data len = %d\n", data_len);

        if (!EVP_SignUpdate(ctx, data, data_len))
        {
            fprintf(stderr, "EVP_SignUpdate: failed.\n");
            EVP_PKEY_free(priv_key);
            return 3;
        }
        printf( "now to sign final..\n");

digest = malloc(EVP_PKEY_size(privkey)); 


        if (!EVP_SignFinal(ctx, &digest, &sig_len, priv_key))
        {
            fprintf(stderr, "EVP_SignFinal: failed.\n");
            free(digest);
            EVP_PKEY_free(priv_key);
            return 3;
        }

        free(data);
        
       // free(digest);
        EVP_PKEY_free(priv_key);
        return EXIT_SUCCESS;

}