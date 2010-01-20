/*
 * $Id$
 *
 * Copyright (c) 2009 .SE (The Internet Infrastructure Foundation).
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

/*
 * This tool sorts a zone file for OpenDNSSEC. It tries to be quick.
 */
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define MAX_LINE_LEN 65535

#if 0
#define DEBUGF(...) printf(__VA_ARGS__)
#else
#define DEBUGF(...)
#endif

#define START_LINE_COUNT 131072 /* 2^17 */

struct mmap {
    void* ptr;
    int size;
};

struct global_data {
    int linecount;
    int listsize;
    char** lines;

    int extracount;
    int extrasize;
    char** extralines;

    int bufcount;
    int arraysize;
    struct mmap* buffers;
};

bool inside_string(char* start, char* pos)
{
    bool inside = false;
    char* p = start;
    while (p < pos) {
        if (*p == '"') {
            if (p > start) {
                if (p[-1] != '\\')
                    inside = !inside;
            }
            else {
                printf("String starter (\") found at column 1!\n");
                exit(-1);
            }
        }
        p++;
    }

    return inside;
}

bool is_rrclass(const char* s)
{
    /*
      This function is used to check if the string is a class name, as opposed
      to a type name. Recognized class names: IN, CH, CS, HS, CLASSxx
    */

    switch (s[0]) {
        case 'C':
            switch (s[1]) {
                case 'H':
                case 'S':
                case 'L':
                    return true;
            }
            break;

        case 'H':
            /* ensure it is not type HINFO */
            if (s[1] == 'S')
                return true;
            break;

        case 'I':
            if (s[1] == 'N')
                return true;
            break;
    }
    return false;
}

bool rr_needs_expansion(int type, char* rr)
{
    /* skip over rr type */
    while (!isspace(*rr))
        rr++;
    while (isspace(*rr))
        rr++;

    /* generic data is not expanded (RFC3597) */
    if (rr[0] == '\\' && rr[1] == '#')
        return false;

    int i;
    switch (type) {
        /* one of more name fields */
        case 2: /* NS */
        case 5: /* CNAME */
        case 7: /* MB */
        case 8: /* MG */
        case 9: /* MR */
        case 12: /* PTR */
        case 14: /* MINFO */
        case 17: /* RP */
        case 39: /* DNAME */
            do {
                if (*rr == '@')
                    return true;
                
                while (*rr && !isspace(*rr))
                    rr++;
                if (rr[-1] != '.')
                    return true;
                while (*rr && isspace(*rr))
                    rr++;
                if (*rr == ';')
                    break;
            } while (*rr);
            break;

            /* one number + one or more name fields */
        case 15: /* MX */
        case 18: /* AFSDB */
        case 21: /* RT */
        case 26: /* PX */
        case 36: /* KX */
            /* skip number */
            while (!isspace(*rr))
                rr++;
            while (isspace(*rr))
                rr++;

            do {
                if (*rr == '@')
                    return true;

                while (*rr && !isspace(*rr))
                    rr++;
                if (rr[-1] != '.')
                    return true;
                while (*rr && isspace(*rr))
                    rr++;
                if (*rr == ';')
                    break;
            } while (*rr);
            break;

        case 6: /* SOA - two names, then verbatim copy*/
            for (i=0; i<2; i++) {
                if (*rr == '@')
                    return true;

                while (!isspace(*rr))
                    rr++;
                if (rr[-1] != '.')
                    return true;
                while (isspace(*rr))
                    rr++;
            }
            break;

        case 33: /* SRV - three numbers, one name */
            for (i=0; i<3; i++) {
                while (!isspace(*rr))
                    rr++;
                while (isspace(*rr))
                    rr++;
            }
            if (*rr == '@')
                return true;
            while (*rr && !isspace(*rr))
                rr++;
            if (rr[-1] != '.')
                return true;
            break;
    }

    return false;
}

/* copy up to <max> names, and append origin if name is relative */
void copy_names(char** src, char** dest, char* origin, int max)
{
    char* s = *src;
    char* d = *dest;
    int count = 0;
    int i;

    while (*s) {
        if (*s == '@') {
            for (i=0; origin[i] && !isspace(origin[i]); i++)
                *d++ = origin[i];
            if (d[-1] != '.')
                *d++ = '.';

            s++;
        }
        else {
            /* copy name */
            while (*s && !isspace(*s))
                *d++ = *s++;

            /* relative name? */
            if (d[-1] != '.') {
                /* append origin */
                *d++ = '.';
                for (i=0; origin[i] && !isspace(origin[i]); i++)
                    *d++ = origin[i];

                /* did origin have an end dot? */
                if (d[-1] != '.')
                    *d++ = '.';
            }
        }
        *d++ = ' ';
        while (isspace(*s))
            s++;
    
        /* abort on comment */
        if (*s == ';')
            break;

        if (max && ++count >= max)
            break;
    }
    *d = 0;

    *src = s;
    *dest = d;
}

/* copy RR, inserting $ORIGIN where needed */
void copy_rr(int type, char* src, char* dest, char* origin)
{
    /* copy rr type */
    while (!isspace(*src))
        *dest++ = *src++;
    *dest++ = ' ';
    while (isspace(*src))
        src++;

    /* generic data is not expanded (RFC3597) */
    if (src[0] == '\\' && src[1] == '#')
        type = 0;

    /* handle RDATA for each type */
    switch (type) {
        case 2: /* NS */
        case 5: /* CNAME */
        case 7: /* MB */
        case 8: /* MG */
        case 9: /* MR */
        case 12: /* PTR */
        case 14: /* MINFO */
        case 17: /* RP */
        case 39: /* DNAME */
            /* one or more name fields */
            copy_names(&src, &dest, origin, 0);
            break;

        case 15: /* MX */
        case 18: /* AFSDB */
        case 21: /* RT */
        case 26: /* PX */
        case 36: /* KX */
            /* one number + one or more name fields */
            while (!isspace(*src))
                *dest++ = *src++;
            *dest++ = ' ';
            while (isspace(*src))
                src++;

            copy_names(&src, &dest, origin, 0);
            break;

        case 6: /* SOA */
            /* two names, then verbatim copy*/
            copy_names(&src, &dest, origin, 2);
            *dest++ = ' ';
            strcpy(dest, src);
            break;

        case 33: /* SRV */
        {
            /* three numbers, one name */
            int i;
            for (i=0; i<3; i++) {
                while (!isspace(*src))
                    *dest++ = *src++;
                *dest++ = ' ';
                while (isspace(*src))
                    src++;
            }
            copy_names(&src, &dest, origin, 1);
            break;
        }

        default:
            /* verbatim copy */
            strcpy(dest, src);
            break;
    }
}

int parse_rrtype(const char* s)
{
    /*
      Performs the minimum necessary checks to decide which RR type 's' is.
      
      The following RR types are not allowed, and hence not matched:
       NULL, OPT, TKEY, TSIG, IXFR, AXFR, MAILB, MAILA, *
    */

    switch (s[0]) {
        case 'A':
            switch (s[1]) {
                case '6': return 38;  /* A6 */
                case 'A': return 28;  /* AAAA */
                case 'F': return 18;  /* AFSDB */
                case 'P': return 42;  /* APL */
                case 'T': return 34;  /* ATMA */
                default: return 1;   /* A */
            }
            break;
            
        case 'C':
            switch (s[1]) {
                case 'E': return 37; /* CERT*/
                case 'N': return 5;  /* CNAME */
            }
            break;

        case 'D':
            switch (s[1]) {
                case 'L': return 32769; /* DLV */
                case 'N':
                    switch (s[2]) {
                        case 'A': return 39; /* DNAME */
                        case 'S': return 48; /* DNSKEY */
                    }
                    break;
                case 'S': return 43; /* DS */
                case 'H': return 49; /* DHCID */
            }
            break;

        case 'E': return 31; /* EID */

        case 'G':
            switch (s[1]) {
                case 'I': return 102; /* GID */
                case 'P': return 27; /* GPOS */
            }
            break;
            
        case 'H':
            return 13; /* HINFO */

        case 'I':
            switch (s[1]) {
                case 'P': return 45;  /* IPSECKEY */
                case 'S': return 20;  /* ISDN */
            }
            break;

        case 'K':
            switch (s[1]) {
                case 'E': return 25; /* KEY */;
                case 'X': return 36; /* KX */
            }
            break;

        case 'L': return 29; /* LOC */

        case 'M':
            switch (s[1]) {
                case 'B': return 7;  /* MB */
                case 'D': return 3;  /* MD */
                case 'F': return 4;  /* MF */
                case 'G': return 8;  /* MG */
                case 'I': return 14; /* INFO */
                case 'R': return 9;  /* MR */
                case 'X': return 15; /* MX */
            }
            break;

        case 'N':
            switch (s[1]) {
                case 'A': return 35; /* NAPTR */

                case 'I':
                    switch (s[2]) {
                        case 'M': return 32; /* NIMLOC */
                        case 'N': return 56; /* NINFO */
                    }
                    break;
                    
                case 'S':
                    switch (s[2]) {
                        case 'A': /* NSAP* */
                            switch (s[4] == '_') {
                                case '_': return 23; /* NSAP_PTR */
                                default: return 22;  /* NSAP */
                            }
                            break;

                        case 'E':
                            switch (s[4]) {
                                case '3':
                                    switch (s[5]) {
                                        case 'P': return 51; /* NSEC3PARAM */
                                        default: return 50;  /* NSEC3 */
                                    }
                                    break;

                                default:
                                    return 47; /* NSEC */
                            }
                        default:
                            return 2;  /* NS */
                    }
                    break;

                case 'U': return 10; /* NULL */
                case 'X': return 30; /* NXT */
            }
            break;

        case 'P':
            switch (s[1]) {
                case 'T': return 12; /* PTR */
                case 'X': return 26; /* PX */
            }
            break;

        case 'R':
            switch (s[1]) {
                case 'K': return 57; /* RKEY */
                case 'P': return 17; /* RP */
                case 'R': return 46; /* RRSIG */
                case 'T': return 21; /* RT */
            }
            break;

        case 'S':
            switch (s[1]) {
                case 'I':
                    switch (s[2]) {
                        case 'G': return 24; /* SIG */
                        case 'N': return 40; /* SINK */
                    }
                    break;
                case 'O': return 6;  /* SOA */
                case 'P': return 99; /* SPF */
                case 'R': return 33; /* SRV */
                case 'S': return 44; /* SSHFP */
            }
            break;

        case 'T':
            switch (s[1]) {
                case 'A': return 32768; /* TA */;
                case 'X': return 16;  /* TXT */
                case 'Y': return atoi(s+4); /* TYPExx */
            }
            break;

        case 'U':
            switch (s[1]) {
                case 'I': 
                    switch (s[2]) {
                        case 'D': return 101; /* UID */
                        case 'N': return 100; /* UINFO */
                    }
                    break;

                case 'N': return 103; /* UNSPEC */
            }
            break;

        case 'W': return 11; /* WKS */
        case 'X': return 19; /* X25 */
    }

    return 0;
}

/* string comparison function for use by qsort() */
int canonical_compare(const void* v1, const void* v2)
{
    char* s1 = *(char**)v1;
    char* s2 = *(char**)v2;
    
    /*** compare name ***/
    char* e1 = strpbrk(s1, " \t");
    char* e2 = strpbrk(s2, " \t");

    /* float errors to top */
    if (!e1)
        return -1;
    if (!e2)
        return 1;

    char* d1 = e1-1;
    char* d2 = e2-1;

    while (1) {
        char* t1 = d1;
        char* t2 = d2;

        /* temporarily terminate segment */
        *t1 = 0;
        *t2 = 0;

        /* find start of name segment */
        while (d1 > s1 && d1[-1] != '.')
            d1--;
        while (d2 > s2 && d2[-1] != '.')
            d2--;

        /* compare name segment */
        int diff = strcasecmp(d1, d2);

        /* un-terminate segment */
        *t1 = '.';
        *t2 = '.';

        if (diff)
            return diff;

        /* equal names? */
        if (d1 == s1 && d2 == s2)
            break;
        
        if (d1 == s1)
            return -1;

        if (d2 == s2)
            return 1;

        /* go to next name segment */
        d1--;
        d2--;
    }

    /*** compare type ***/

    while (isspace(*e1))
        e1++;
    while (isspace(*e2))
        e2++;

    /* skip two fields (ttl and class) */
    int i;
    for (i=0; i<2; i++) {
        while (!isspace(*e1)) 
            e1++;
        while (!isspace(*e2))
            e2++;
        while (isspace(*e1))
            e1++;
        while (isspace(*e2))
            e2++;
    }
    
    int t1 = parse_rrtype(e1);
    int t2 = parse_rrtype(e2);

    if (t1 - t2)
        return t1 - t2;

    /*** compare RDATA ***/

    /* skip type and trailing whitespace */
    while (!isspace(*e1))
        e1++;
    while (!isspace(*e2))
        e2++;
    while (isspace(*e1))
        e1++;
    while (isspace(*e2))
        e2++;

    /* compare RDATA */
    return strcmp(e1, e2);
}

/* read and parse a zone file */
int read_file(char* filename, char* origin, char* default_ttl,
              struct global_data* g)
{
    int infd = open(filename, O_RDONLY);
    if (-1 == infd) {
        perror(filename);
        return -2;
    }

    struct stat statbuf;
    if (fstat(infd, &statbuf)) {
        perror(filename);
        return -3;
    }

    void* buffer = mmap(NULL, statbuf.st_size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE, infd, 0);
    close(infd);
    if (MAP_FAILED == buffer) {
        perror("mmap");
        return -4;
    }        

    /* store the mmap buffer pointer in a global array */
    struct mmap* mbuf;
    if (g->bufcount >= g->arraysize) {
        /* we need to realloc the mmap array */
        g->arraysize *= 2;
        g->buffers = realloc(g->buffers, g->arraysize * sizeof(struct mmap));
        if (!g->buffers) {
            perror("buffers realloc");
            exit(-1);
        }
    }
    mbuf = &(g->buffers[g->bufcount++]);
    mbuf->ptr = buffer;
    mbuf->size = statbuf.st_size;
    
    int listlen = statbuf.st_size / 40; /* guesstimate line count */
    if (g->linecount + listlen > g->listsize) {
        /* we need to realloc the line array */
        while (g->listsize < g->linecount + listlen)
            g->listsize *= 2;

        g->lines = realloc(g->lines, g->listsize * sizeof(char*));
        if (!g->lines) {
            perror("lines realloc");
            exit(-1);
        }
        DEBUGF("Reallocated line list to %d lines\n", g->listsize);
    }

    /* skip over any leading dots */
    while (*origin == '.')
        origin++;

    char* currname = NULL;
    char* currclass = NULL;
    char* currttl = 0;
    char* ttlmacro = 0;
    char* ptr = buffer;
    int linenumber = 1;
    while (1) {
        bool parse = false;

        /* don't store comments or empty lines */
        if (*ptr != ';' && *ptr != '\n' && *ptr != '$') {
            parse = true;
            g->lines[g->linecount++] = ptr;
        }

        /* terminate line */
        char* end = strchr(ptr, '\n');
        if (!end)
            break; /* end of file */
        *end = 0;

        /* handle macros */
        if (*ptr == '$') {
            char* p = ptr;
            while (!isspace(*p))
                p++;
            while (isspace(*p))
                p++;

            switch (ptr[1]) {
                case 'I': {
                    char* filename = p;
                    while (*p && !isspace(*p))
                        p++;
                    *p = 0; /* terminate filename */
                    p++;
                    while (*p && isspace(*p))
                        p++;

                    char* domain = NULL;
                    if (*p && *p != ';') {
                        domain = p;
                        while (*p && !isspace(*p))
                            p++;
                        *p = 0; /* terminate domain name */
                    }
                    read_file(filename, domain, default_ttl, g);
                    break;
                }

                case 'O':
                    origin = p;

                    /* skip over any leading dots */
                    while (*origin == '.')
                        origin++;
                    break;

                case 'T':
                    ttlmacro = p;
                    break;
            }
        }
        
        if (parse) {
            char* p = ptr;

            /*** join split lines ***/

            /* look for multi-line token */
            char* paren = p;
            while ((paren = strchr(paren, '('))) {
                /* was paren part of a quoted string? */
                if (!inside_string(ptr, paren))
                    break;

                /* was paren in a comment? */
                char* comment = strchr(ptr, ';');
                if (comment && paren > comment) {
                    /* paren was in a comment. ignore it. */
                    *comment = 0;
                    paren = NULL;
                    break;
                }
                paren++;
            }
            if (paren) {
                /* multi-line started.
                   connect this line with next (fill to eol with space) */
                memset(paren, ' ', end - paren + 1);

                /* join lines until ')' */
                while (1) {
                    end = strchr(p, '\n');
                    if (!end) {
                        printf("%s:%d: Unclosed parenthesis\n", filename, linenumber);
                        exit(-1);
                    }
                    *end = 0;

                    /* find and remove comment */
                    char* comment = strchr(p, ';');
                    if (comment)
                        *comment = 0;

                    /* look for closing paren */
                    paren = strchr(p, ')');
                    if (paren) {
                        *paren = 0;
                        break;
                    }
                    else {
                        /* no closing paren, connect next line */
                        if (comment)
                            memset(comment, ' ', end - comment + 1);
                        else
                            *end = ' ';
                    }

                    p = end + 1; /* go to next line */
                    linenumber++;
                }
            }


            /*** expand inherited fields ***/

            char* name = NULL;
            char* class = NULL;
            char* rr = NULL;
            char* ttl = NULL;
            int rrtype = 0;
            bool expand = false;
            bool relative_name = false;

            /* check for name */
            p = ptr;
            switch (p[0]) {
                case ' ':
                case '\t':
                    expand = true;
                    name = currname;
                    break;

                case '@':
                    expand = true;
                    name = origin;
                    p++;
                    break;

                default: {
                    name = ptr;

                    /* relative name? */
                    while(!isspace(*p))
                        p++;
                    if (p[-1] != '.') {
                        relative_name = true;
                        expand = true;
                    }
                    else
                        currname = ptr; /* remember last absolute name */
                    break;
                }
            }

            /* see if both TTL and CLASS are here */
            while (1) {
                while(isspace(*p))
                    p++;
                /* is this the ttl? */
                if (isdigit(*p))
                    ttl = p;
                else  {
                    /* is this the class? */
                    if (is_rrclass(p)) {
                        class = p;
                        currclass = p;
                    }
                    else {
                        /* verify rr type */
                        rrtype = parse_rrtype(p);
                        switch (rrtype) {
                            case 46:
                            case 47:
                            case 50:
                            case 51:
                                /* strip this line */
                                g->linecount--;
                                goto next_line;

                            case 0:
                                printf("%s:%d: Unknown RR: %s\n", filename, linenumber, p);
                                exit(-1);
                                break;
                        }
                        rr = p;
                        if (rr_needs_expansion(rrtype, rr))
                            expand = true;
                        break;
                    }
                }
                /* skip past this field */
                while (!isspace(*p))
                    p++;
            }

            if (!ttl || !class)
                expand = true;

            if (expand) {
                /* construct a new line with inherited values */
                char* line = malloc(MAX_LINE_LEN);
                char* l = line;

                /* name */
                if (!name) {
                    name = currname;
                    if (!name) {
                        printf("%s:%d: No name\n", filename, linenumber);
                        exit(-1);
                    }
                }

                currname = l; /* store last absolute name */
                while (*name && !isspace(*name))
                    *l++ = *name++;

                if (relative_name) {
                    if (!origin) {
                        printf("%s:%d: Relative name without origin\n", filename, linenumber);
                        exit(-1);
                    }
                    *l++ = '.';
                        
                    p = origin;
                    while (*p && !isspace(*p))
                        *l++ = *p++;

                }
                if (l[-1] != '.') /* command line-supplied name */
                    *l++ = '.';
                *l++ = ' ';

                /* append ttl */
                if (!ttl) {
                    if (currttl)
                        ttl = currttl;
                    else
                        if (ttlmacro)
                            ttl = ttlmacro;
                        else
                            ttl = default_ttl;
                }
                if (!ttl) {
                    printf("%s:%d: No TTL\n", filename, linenumber);
                    exit(-1);
                }
                while (*ttl && !isspace(*ttl))
                    *l++ = *ttl++;
                *l++ = ' ';

                /* append class */
                if (!class)
                    class = currclass;
                if (!class) {
                    printf("%s:%d: No class\n", filename, linenumber);
                    exit(-1);
                }
                while (*class && !isspace(*class))
                    *l++ = *class++;
                *l++ = ' ';

                copy_rr(rrtype, rr, l, origin);
                
                /* add line to extralines array, so we can free it */
                if (g->extracount >= g->extrasize) {
                    /* expand extra lines array */
                    g->extrasize *= 2;
                    g->extralines = realloc(g->extralines,
                                            g->extrasize * sizeof(char*));
                    if (!g->extralines) {
                        perror("extralines realloc");
                        return -9;
                    }
                }
                g->extralines[g->extracount++] = line;

                /* replace original line in the to-be-sorted array */
                g->lines[g->linecount-1] = line;
            }
        }

      next_line:
        ptr = end + 1; /* go to next line */
        linenumber++;

        if (g->linecount >= g->listsize) {
            g->listsize *= 2;
            g->lines = realloc(g->lines, g->listsize * sizeof(char*));
            if (!g->lines) {
                perror("lines realloc 2");
                return -8;
            }
            DEBUGF("Reallocated line list to %d lines\n", g->listsize);
        }
    }

    return 0;
}

void init_global_data(struct global_data* g)
{
    g->linecount = 0;
    g->listsize = START_LINE_COUNT;
    g->lines = malloc(g->listsize * sizeof(char*));
    if (!g->lines) {
        perror("lines malloc");
        exit(-1);
    }

    g->extracount = 0;
    g->extrasize = 10;
    g->extralines = malloc(g->extrasize * sizeof(char*));
    if (!g->extralines) {
        perror("extralines malloc");
        exit(-1);
    }

    g->bufcount = 0;
    g->arraysize = 1; /* low start value to force realloc testing */
    g->buffers = malloc(g->arraysize * sizeof(struct mmap));
    if (!g->buffers) {
        perror("buffers malloc");
        exit(-1);
    }
}

int main(int argc, char* argv[])
{
    const char* help =
        "usage: quicksorter -f INFILE -w OUTFILE [OPTIONS] \n"
        "options:\n"
        "-m <min>\tSOA minimum\n"
        "-o <origin>\tZone origin\n";

    char* default_ttl = NULL;
    char* infile = NULL;
    char* outfile = NULL;
    char* origin = NULL;
    int c;
    while ((c = getopt(argc, argv, "f:w:m:o:")) != -1) {
        switch (c) {
            case 'f':
                infile = optarg;
                break;

            case 'w':
                outfile = optarg;
                break;

            case 'm':
                default_ttl = optarg;
                break;

            case 'o':
                origin = optarg;
                break;
        }
    }

    if (!infile || !outfile) {
        printf("%s\n", help);
        return -1;
    }

    /* open outfile before reading and sorting,
       to make sure we can create it */
    FILE* outf = fopen(outfile, "w");
    if (!outf) {
        perror(outfile);
        return -5;
    }

    struct global_data g;
    init_global_data(&g);

    read_file(infile, origin, default_ttl, &g);
    DEBUGF("Read %d lines\n", g.linecount);

    qsort(g.lines, g.linecount, sizeof (char*), canonical_compare);

    int i;
    for (i=0; i<g.linecount; i++)
        if (g.lines[i][0])
            fprintf(outf, "%s\n", g.lines[i]);
    fclose(outf);

    /* free all data */
    free(g.lines);
    for (i=0; i<g.extracount; i++)
        free(g.extralines[i]);
    free(g.extralines);
    for (i=0; i<g.bufcount; i++)
        munmap(g.buffers[i].ptr, g.buffers[i].size);
    free(g.buffers);
    
    return 0;
}
