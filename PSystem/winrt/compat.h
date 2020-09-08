// simply compatiblilty mappings for compiling the p-code interpreter
// Copyright (C) 2013 Frank Seide

// TODO: pull into here:
//  - libexplain fakes
//  - LIB_GCC_ATTRIBUTES that are actually needed; then put the #include into an #ifdef for GCC

#define _CRT_SECURE_NO_WARNINGS 1
//#define inline __inline
typedef int ssize_t;    // return value of _read()

static void sleep(int n) {} // cannot do this in WinRT, need a different solution

#include <stdio.h>
#include <stdlib.h>
static void invalid_on_winrt(const char * what)
{
    fprintf (stderr, "invalid usage of %s in WinRT\n", what);
    exit(EXIT_FAILURE);
}

static FILE * popen(const char * f, const char * m)
{
    invalid_on_winrt("popen");
    return NULL;
}

static int pclose(FILE * f)
{
    invalid_on_winrt("pclose");
    return -1;
}

typedef struct timeval  // sys/time.h --where is this in WinRT?
{
    long int tv_sec;
    long int tv_usec;
} timeval;

static int explain_gettimeofday_on_error(timeval * tv, void * null)
{
    // TODO: emulate this properly
    tv->tv_sec = 0;
    tv->tv_usec = 0;
    return 0;       // -1 means failure
};

static int round(float f)
{
    if (f > 0)
        return (int) (f + 0.5);
    else
        return -(int) (-f + 0.5);
}

typedef struct option       // never used this...
{
    const char * longname;
    int v1;
    int v2;
    char c;
} option;
