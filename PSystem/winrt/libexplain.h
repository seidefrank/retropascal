// missing libexplain functions, reimagined
// Copyright (C) 2013 Frank Seide

// .. TODO: just make them wrappers with no error printing and move into compat.h

#pragma once

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// print errno
static void explain_and_die(const char * what)
{
    errno_t eno = errno;
    char estr[128];
    strerror_s(estr, sizeof(estr)/sizeof(*estr), eno);
    fprintf (stderr, "%s failure: %s (%d)\n", what, estr, eno);
    exit(EXIT_FAILURE);
}

static int explain_open_or_die (const char * FileName, int flags, int mode)
{
    int fd = _open(FileName, flags, mode);
    if (fd == -1)
        explain_and_die("_open");
    return fd;
}

static int explain_read_on_error (int fd, void * buf, int sz)
{
    return _read(fd, buf, sz);
}

static int explain_fstat_on_error(int fd, struct stat * buf)
{
    return fstat(fd, buf);
}

static FILE * explain_fopen_or_die (const char * FileName, const char * flags)
{
    FILE * f = fopen(FileName, flags);
    if (f == NULL)
        explain_and_die("fopen");
    return f;
}

static void explain_fclose_or_die (FILE * f)
{
    if (fclose(f) != 0)
        explain_and_die("fclose");
}
