/*
 * $Id$
 *
 * Copyright (c) 2010 .SE (The Internet Infrastructure Foundation).
 * All rights reserved.
 *
 * Written by Björn Stenberg <bjorn@haxx.se> for .SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>

/* Internal binary RR format:

   ttl      - (int) ttl value.
   owner    - owner name, stored backwards (i.e. 'com.example.www').
              each character is stored in a (short), with these specials:
               -2 = name segment delimiter ('.')
               -1 = end of name
   type     - (unsigned short) RR type
   class    - (unsigned short) RR class
   rdata    - binary RDATA in wire format

   The purpose of using this custom format instead of the exact wire format is
   to enable fast comparison. This way, RR records can be compared using a
   single memcmp() starting at the owner field.
*/

#define MAX_LINE_LEN 65535
#define MAX_NAME_LEN 255

#define NUM_TYPES 101
#define NUM_CLASSES 5

static const char* typename[NUM_TYPES] = {
    NULL, "A", "NS", "MD", "MF", "CNAME", "SOA", "MB", "MG", "MR",
    "NULL","WKS","PTR","HINFO","MINFO","MX","TXT","RP","AFSDB","X25",
    "ISDN","RT","NSAP","NSAP-PTR","SIG","KEY","PX","GPOS","AAAA","LOC",
    "NXT","EID","NIMLOC","SRV","ATMA","NAPTR","KX","CERT","A6","DNAME",
    "SINK","OPT","APL","DS","SSHFP","IPSECKEY","RRSIG","NSEC","DNSKEY","DHCID",
    "NSEC3","NSEC3PARAM",NULL,NULL,NULL,"HIP","NINFO","RKEY",NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,"SPF","DLV"
};

static const char* classname[NUM_CLASSES] = {
    NULL, "IN", NULL, "CH", "HS"
};

enum {
    RD_INT8,
    RD_INT16,
    RD_INT32,
    RD_NAME,   /* wire format, one string per segment */
    RD_STRING, /* wire format */
    RD_A,
    RD_AAAA,
    RD_LOC,    /* TODO: RFC 1876 */
    RD_BASE64, /* TODO: RFC 4398 (including whitespace!) */
    RD_BASE16, /* TODO: RFC 3658 (hex) */
};

static const char format_list[][8] = {
    /* 0 */         { 0 },
    /* 1: A  */     { 1, RD_A },
    /* 2: NS */     { 1, RD_NAME },
    /* 3: MD */     { 1, RD_NAME },
    /* 4: MF */     { 1, RD_NAME },
    /* 5: CNAME */  { 1, RD_NAME },
    /* 6: SOA */    { 7, RD_NAME, RD_NAME,
                      RD_INT32, RD_INT32, RD_INT32, RD_INT32, RD_INT32 },
    /* 7: MB */     { 1, RD_NAME },
    /* 8: MG */     { 1, RD_NAME },
    /* 9: MR */     { 1, RD_NAME },
    /* 10: NULL */  { 0 }, /* not supported by OpenDNSSEC */
    /* 11: WKS */   { 0 }, /* not supported by OpenDNSSEC */
    /* 12: PTR */   { 1, RD_NAME },
    /* 13: HINFO */ { 2, RD_STRING, RD_STRING },
    /* 14: MINFO */ { 2, RD_NAME, RD_NAME },
    /* 15: MX */    { 2, RD_INT16, RD_NAME },
    /* 16: TXT */   { 1, RD_STRING },
    /* 17: RP */    { 2, RD_NAME, RD_NAME },
    /* 18: AFSDB */ { 2, RD_INT16, RD_NAME },
    /* 19: X25 */   { 1, RD_STRING },
    /* 20: ISDN */  { 2, RD_STRING, RD_STRING },
    /* 21: RT */    { 2, RD_INT16, RD_STRING },
    /* 22: NSAP */  { 1, RD_STRING },
    /* 23: NSAP-PTR */ {0}, /* not supported by OpenDNSSEC */
    /* 24: SIG */   { 0 }, /* not supported by OpenDNSSEC */
    /* 25: KEY */   { 0 }, /* not supported by OpenDNSSEC */
    /* 26: PX */    { 3, RD_INT16, RD_NAME, RD_NAME },
    /* 27: GPOS */  { 0 }, /* not supported by OpenDNSSEC */
    /* 28: AAAA */  { 1, RD_AAAA },
    /* 29: LOC */   { 1, RD_LOC },
    /* 30: NXT */   { 0 }, /* not supported by OpenDNSSEC */
    /* 31: EID */   { 0 }, /* not supported by OpenDNSSEC */
    /* 32: NIMLOC */{ 0 }, /* not supported by OpenDNSSEC */
    /* 33: SRV */   { 4, RD_INT16, RD_INT16, RD_INT16, RD_NAME },
    /* 34: ATMA */  { 0 },  /* not supported by OpenDNSSEC */
    /* 35: NAPTR */ { 6, RD_INT16, RD_INT16,
                      RD_STRING, RD_STRING, RD_STRING, RD_NAME },
    /* 36: KX */    { 2, RD_INT16, RD_NAME },
    /* 37: CERT */  { 4, RD_INT16, RD_INT16, RD_INT8, RD_BASE64 },
    /* 38: A6 */    { 0 }, /* not supported by OpenDNSSEC */
    /* 39: DNAME */ { 1, RD_NAME },
    /* 40: SINK */  { 0 }, /* not supported by OpenDNSSEC */
    /* 41: OPT */   { 0 }, /* not allowed in master file */
    /* 42: APL */   { 0 }, /* not supported by OpenDNSSEC */
    /* 43: DS */    { 4, RD_INT16, RD_INT8, RD_INT8, RD_BASE16 },
    /* 44: SSHFP */ { 3, RD_INT8, RD_INT8, RD_BASE16 },
    /* **FIXME** 45: IPSECKEY */ {5, RD_INT8, RD_INT8, RD_INT8, RD_STRING, RD_STRING},
    /* 46: RRSIG */ { 0 }, /* OpenDNSSEC will discard this DNSSEC RR */
    /* 47: NSEC */  { 0 }, /* OpenDNSSEC will discard this DNSSEC RR */
    /* **FIXME** 48: DNSKEY */{ 4, RD_INT16, RD_INT8, RD_INT8, RD_STRING },
    /* 49: DHCID */ { 1, RD_BASE64 },
    /* 50: NSEC3 */ { 0 }, /* OpenDNSSEC will discard this DNSSEC RR */
    /* 51: NSEC3PARAM */ { 0 }, /* OpenDNSSEC will discard this DNSSEC RR */
    /* 52: Unassigned */ { 0 },
    /* 53: Unassigned */ { 0 },
    /* 54: Unassigned */ { 0 },
    /* 55: HIP */   { 0 }, /* not supported by OpenDNSSEC */
    /* 56: NINFO */ { 0 }, /* not supported by OpenDNSSEC */
    /* 57: RKEY */  { 0 }, /* not supported by OpenDNSSEC */
    /* 58: Unassigned */ { 0 },
    /* 59: Unassigned */ { 0 },
    /* 60-69: unassigned*/ {0},{0},{0},{0},{0},{0},{0},{0},{0},{0},
    /* 70-79: unassigned*/ {0},{0},{0},{0},{0},{0},{0},{0},{0},{0},
    /* 80-89: unassigned*/ {0},{0},{0},{0},{0},{0},{0},{0},{0},{0},
    /* 90-97: unassigned*/ {0},{0},{0},{0},{0},{0},{0},{0},{0},
    /* 99: SPF */   { 1, RD_STRING },
    /* 100: Used for 32769: DLV */ { 4, RD_INT16, RD_INT8, RD_INT8, RD_BASE16 }
};

static inline void encode_int32(unsigned int val, char* dest)
{
    *((unsigned int*)dest) = val;
}

static inline unsigned int decode_int32(char* src)
{
    return *((unsigned int*)src);
}

static inline void encode_int16(unsigned int val, char* dest)
{
    *((unsigned short*)dest) = val;
}

static inline unsigned int decode_int16(char* src)
{
    return *((unsigned short*)src);
}

static int parse_ttl(char* ttl)
{
    int seconds = 0;

    while (*ttl) {
        while (isspace((unsigned char)*ttl))
            ttl++;
        int val = atoi(ttl);
        while (isdigit((unsigned char)*ttl))
            ttl++;
        switch (tolower(*ttl)) {
            case 'm': val *= 60; break;
            case 'h': val *= 3600; break;
            case 'd': val *= 86400; break;
            case 'w': val *= 86400*7; break;
            default:
                break;
        }
        seconds += val;
        ttl++;
    }
    return seconds;
}

static void encode_string(char** _src, char** _dest, bool domain_name)
{
    char* src = *_src;
    char* dest = *_dest;
    int len = 1; /* first byte is length */
    bool quoted = false;
    while (*src && (quoted || !isspace((unsigned char)*src))) {
        switch (*src) {
            case '\"':
                quoted = !quoted;
                dest[len++] = *src;
                break;

            case '\\':
                if (isdigit((unsigned char)src[1])) {
                    dest[len++] =
                        (src[1] - 48) * 100 +
                        (src[2] - 48) * 10 +
                        (src[3] - 48);
                    src += 3;
                }
                else {
                    dest[len++] = src[1];
                    src++;
                }
                break;

            case '.':
                if (domain_name) {
                    dest[0] = len - 1;
                    dest += len;
                    len = 1;
                }
                else
                    dest[len++] = *src;
                break;

            default:
                if (domain_name)
                    dest[len++] = tolower((unsigned char)*src);
                else
                    dest[len++] = *src;
                break;
        }
        src++;
    }

    dest[0] = len - 1;
    dest += len;

    /* null terminate label serie
    if (domain_name)
        *dest++ = 0; */

    if (len > 255) {
        printf("String '%s' is too long! (%d bytes)\n", src, len);
        exit(-1);
    }

    *_src = src;
    *_dest = dest;
}

static void decode_string(char** _src, char** _dest, bool domain_name)
{
    char* src = *_src;
    char* dest = *_dest;
    
    do {
        bool quoted = false;
        int len = (unsigned char)*src++;
        
        int i;
        for (i=0; i<len; i++) {
            if (*src == '\"')
                quoted = !quoted;

            if (isprint((unsigned char)*src))
                *dest++ = *src;
            else
                dest += sprintf(dest, "\\%03d", *src);
            src++;
        }
        if (domain_name)
            *dest++ = '.';
    } while (domain_name && *src);

    if (domain_name)
        src++;

    *_src = src;
    *_dest = dest;
}

static void* encode_owner(char* name, void* dest)
{
    char tmpname[MAX_NAME_LEN];
    char labelpos[MAX_NAME_LEN];
    char* tmpptr = tmpname;
    encode_string(&name, &tmpptr, true);

    /* iterate through string and store the position of each label */
    int count = 0;
    int pos = 0;
    do {
        labelpos[count++] = pos;
        pos += tmpname[pos] + 1;
    } while (tmpname[pos]);

    /* now store the name backwards */
    short* dptr = dest;
    int i;
    for (i=count-1; i >= 0; i--) {
        char* sptr = tmpname + labelpos[i];
        int len = *sptr++;
        while (len--)
            *dptr++ = *sptr++;
        *dptr++ = -2;
    }
    *dptr++ = -1;

    return dptr;
}

static void decode_owner(char** _rr, char** _dest)
{
    short* rr = (short*)*_rr;
    char* dest = *_dest;

    int len = 0;
    while (rr[len] != -1)
        len++;

    /* reverse name */
    short* delim = rr + len - 1;

    while (1) {
        short* end = delim;

        /* find start of name segment */
        while (delim > rr && (delim[-1] != -2))
            delim--;

        short* p = delim;
        
        /* copy segment */
        while (p < end)
            *dest++ = *p++;
        *dest++ = '.';

        if (delim == rr)
            break;

        delim--;
    }
    *_rr += (len + 1) * sizeof(short);
    *_dest = dest;
}

void encode_int(char** src, char** dest, int type)
{
    int val = atoi(*src);
    switch (type) {
        case RD_INT8:
            **dest = val;
            (*dest)++;
            break;

        case RD_INT16:
            encode_int16(val, *dest);
            (*dest) += 2;
            break;

        case RD_INT32:
            encode_int32(val, *dest);
            (*dest) += 4;
            break;
    }

    while (**src && !isspace((unsigned)**src))
        (*src)++;
}

static void* encode_rdata(int type, char* rdata, char* dest)
{
    const char* format = format_list[type];
    int pcount = format[0];

    int i;
    for (i=1; i <= pcount; i++) {
        switch (format[i]) {
            case RD_NAME:
                encode_string(&rdata, &dest, true);
                break;

            case RD_STRING:
                encode_string(&rdata, &dest, false);
                break;

            case RD_A:
                inet_pton(AF_INET, rdata, dest);
                while (isspace((unsigned char)*rdata))
                    rdata++;
                dest += 4;
                break;

            case RD_AAAA:
                inet_pton(AF_INET6, rdata, dest);
                while (isspace((unsigned char)*rdata))
                    rdata++;
                dest += 16;
                break;
                
            case RD_INT8:
            case RD_INT16:
            case RD_INT32:
                encode_int(&rdata, &dest, format[i]);
                break;

            default:
                printf("Error! Unsupported type %d.\n", format[i]);
                exit(-1);
                break;
        }
        while (isspace((unsigned char)*rdata))
            rdata++;
    }

    return dest;
}

static void decode_rdata(int type, char* rdata, char* dest)
{
    const char* format = format_list[type];
    int pcount = format[0];

    int i;
    for (i=1; i <= pcount; i++) {
        switch (format[i]) {
            case RD_NAME:
                decode_string(&rdata, &dest, true);
                break;

            case RD_STRING:
                decode_string(&rdata, &dest, false);
                break;

            case RD_A:
                inet_ntop(AF_INET, rdata, dest, INET_ADDRSTRLEN);
                rdata += 4;
                while (*dest)
                    dest++;
                break;

            case RD_AAAA:
                inet_ntop(AF_INET6, rdata, dest, INET6_ADDRSTRLEN);
                rdata += 16;
                while (*dest)
                    dest++;
                break;
                
            case RD_INT8:
                dest += sprintf(dest, "%d", *rdata);
                rdata++;
                break;

            case RD_INT16:
                dest += sprintf(dest, "%d", decode_int16(rdata));
                rdata += 2;
                break;

            case RD_INT32:
                dest += sprintf(dest, "%d", decode_int32(rdata));
                rdata += 4;
                break;
        }
        if (i<pcount)
            *dest++ = ' ';
    }
    *dest = 0;
}

int encode_rr(char* name, int type, int class, char* ttl, char* rdata,
               char* dest)
{
    char* tmp;

    int seconds = parse_ttl(ttl);
    encode_int32(seconds, dest);

    tmp = dest + 4;
    char* ptr = encode_owner(name, (short*)(dest+4));

    encode_int16(type, ptr);
    ptr += 2;

    encode_int16(class, ptr);
    ptr += 2;

    tmp = ptr;
    ptr = encode_rdata(type, rdata, ptr);

    return ptr - dest;
}

void decode_rr(char* src, char* dest)
{
    int ttl = decode_int32(src);
    src += 4;

    decode_owner(&src, &dest);
    *dest++ = ' ';

    dest += sprintf(dest, "%d ", ttl);

    int type = decode_int16(src);
    src += 2;
    int class = decode_int16(src);
    src += 2;

    if (class > 0 && class < NUM_CLASSES)
        dest += sprintf(dest, "%s ", classname[class]);
    else
        dest += sprintf(dest, "CLASS%d ", type);

    if (type == 32769) /* special case for DLV */
        type = 100;
    if (type > 0 && type < NUM_TYPES)
        dest += sprintf(dest, "%s ", typename[type]);
    else
        dest += sprintf(dest, "TYPE%d ", type);
        
    decode_rdata(type, src, dest);
}

void hexdump(void* ptr, int count)
{
    const int DUMP_LINE_LEN = 16;
    unsigned char* buffer = ptr;

    int l;
    for (l=0; l<=count/DUMP_LINE_LEN; l++) {
        int start = l*DUMP_LINE_LEN;
        int end = start + DUMP_LINE_LEN;
        if ( end > count )
            end = count;

        if ( start == end )
            break;

        printf("%p: ", ptr + l*DUMP_LINE_LEN);

        /* print hex code */
        int i;
        for (i=start; i<start+DUMP_LINE_LEN; i++ ) {
            if ( i < end )
                printf("%02x ", buffer[i] );
            else
                printf("   ");
        }

        /* print characters */
        for (i=start; i<end; i++ ) {
            if ( isprint( buffer[i] ) )
                printf("%c", buffer[i] );
            else
                printf(".");
        }
        printf("\n");
    }
}


int main(void)
{
    char buf1[MAX_LINE_LEN];
    char buf2[MAX_LINE_LEN];

    memset(buf1, 0x55, sizeof(buf1));
    int len = encode_rr("www.example.com.", 99, 1, "2d1h", "\"quoted string\"", buf1);
    hexdump(buf1, len);

    memset(buf2, 0xaa, sizeof(buf2));
    decode_rr(buf1, buf2);
    hexdump(buf2, strlen(buf2)+1);
    return 0;
}
