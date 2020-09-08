// p-system file system emulator to allow direct access to a Windows folder--TEXT conversions
// Copyright (C) 2013 Frank Seide

#define _CRT_SECURE_NO_WARNINGS 1

#include "converttext.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/utime.h>
#include <sys/stat.h>
#include <time.h>
#include <string>

using namespace std;

// return false if 'which' does not exist or is older than 'wrtwhich'
// if 'wrtwhich' does not exist, then 'which' is not checked & considered up-to-date no matter what
static bool fuptodate(const char * which, const char * wrtwhich)
{
    struct _stat64i32 wrtstat;
    if (_stat64i32(wrtwhich, &wrtstat) != 0)
        return true;        // 'wrtwhich' does not exist (or is otherwise inaccessible): consider 'which' up-to-date
    struct _stat64i32 whichstat;
    if (_stat64i32(which, &whichstat) != 0)
        return false;       // 'which' does not exist: consider it out of date (needs to be created)
    // both exist: compare by date
    bool isuptodate = _difftime64(whichstat.st_mtime, wrtstat.st_mtime) >= 0;
    // BUGBUG: This fails for newly created files. Maybe due to rename()?
    return isuptodate;  // true if 'which' is not older than 'wrtwhich'
}

// copy the file modification time
static bool fcpmtime(const char * from, const char * to)
{
    struct _stat64i32 stat;
    if (_stat64i32(from, &stat) != 0)
        return false;
    __utimbuf64 timbuf;
    timbuf.actime = stat.st_mtime;  // use mod time as access time
    timbuf.modtime = stat.st_mtime;
    return _utime64(to, &timbuf) == 0;
}

// read a line in Windows format and convert it into UCSD
//  - DLE at start of line
//  - line endings are converted into CR in any case
//  - last line has line ending appended as that is required for the file format
// returns empty string if no more characters
template<typename GETCFN>
static bool readandconvertline(string & line, GETCFN & getc)
{
    line.clear();
    for(;;)
    {
        int ch = getc();
        if (ch == -2/*err*/)
            return false;
        else if (ch == -1/*eof*/)
        {
            if (line.empty())           // if we enter with EOF condition, we will return 'true' and an empty string
                return true;
            else
                break;                  // EOF in middle of line--we just return what we got, and will detect feof() in next call
        }
        else if (ch == '\r')            // we just ignore all CR characters
            continue;
        else if (ch == '\n')            // end of line
            break;                      // CR will be added outside
#if 0   // procs.a.text contains a TAB character, i.e. the system is well able to handle it; don't convert
        else if (ch == '\t')
        {
            size_t tabstop = (line.size() / 8 + 1) * 8;
            line.append(tabstop - line.size(), ' ');
        }
        else if (ch != 12 && ch != '\t' && (ch < 32 || ch > 127))   // we ignore all that cannot be encoded, except FF
            continue;
#else
        else if (ch <= 0 || ch >= 256)      // 0 is for structure; >= 256 cannot be represented: skip
            continue;
#endif
        else                                // keep everything else
            line.push_back(ch);
    }
    // compress line starts with DLE (0x10)
    size_t numspaces = 0;
    for (numspaces = 0; numspaces < line.size() && line[numspaces] == ' ' && numspaces + 32 < 128/*max encodable*/; numspaces++)
        ;
    if (numspaces > 2)  // encode as run length
    {
        line[0] = 0x10; // DLE
        line[1] = (char) (numspaces + 32);
        line.erase(line.begin() + 2, line.begin() + numspaces);
    }
    // append the CR --we do this always because the compiler expects it
    line.push_back('\r');   
    return true;
}

bool converttoTEXT(const char * from, const char * to, bool make)
{
    if (make && fuptodate(to, from))    // file is up-to-date
        return true;
    string totmp = string(to) + ".tmp$$";
    bool ok = true;
    FILE * fin = NULL;
    FILE * fout = NULL;
    fin = fopen(from, "rb");
    ok = ok && (fin != NULL);
    if (ok)
        fout = fopen(totmp.c_str(), "wb");
    ok = ok && (fout != NULL);

    // file begins with 1024 zeroes
    const char zeroes[1024] = { 0 };
    // note: ok = ok && uses short-cut evaluation, i.e. does not evaluate if 'ok' is already false
    ok = ok && (fwrite(&zeroes[0], 1024, 1, fout) == 1);

    // copy line by line
    string line;
    size_t charsinblock = 0;
    while(ok)
    {
        // get line
        ok = ok && readandconvertline(line, [&]() -> int
        {
            int ch = fgetc(fin);
            if (ch != EOF)
                return ch;
            else if (ferror(fin))
                return -2;
            else
                return -1;
        });
        if (!ok || line.empty())    // indicates end of file
            break;
        // if line does not fit then fill with zeroes first
        if (charsinblock + line.size() >= 1024) // compiler detects end of block by a NUL character
        {
            ok = ok && (fwrite(&zeroes[0], 1024 - charsinblock, 1, fout) == 1);
            charsinblock = 0;
        }
        // now either the string fits or we are at a boundary
        // deal with extra-long lines --insert CR like Miller's psystem-fs
        while (ok && line.size() > 1023)
        {
            ok = ok && (fwrite(&line[0], 1022, 1, fout) == 1);
            char pad[2] = { '\r', 0 };
            ok = ok && (fwrite(&pad[0], 2, 1, fout) == 1);
            line.erase(line.begin(), line.begin() + 1022);  // and take it out of the string
            // now we are guaranteed again to be on a block boundary
        }
        // write the line --the CR is already part of 'line'
        ok = ok && (fwrite(&line[0], line.size(), 1, fout) == 1);
        charsinblock += line.size();
    }
    ok = ok && (fwrite(&zeroes[0], 1024 - charsinblock, 1, fout) == 1);  // top it off with NULs
    ok = ok && (fflush(fout) == 0);      // fclose() also flushes, but we do it here to check for the error

    // done
    if (fin) fclose(fin);
    if (fout) fclose(fout);
    ok = ok && fcpmtime(from, totmp.c_str());    // copy the file date for future up-to-date checks
    ok = ok && (_unlink(to) == 0 || errno == ENOENT);  // now move: first delete existing if any
    ok = ok && rename(totmp.c_str(), to) == 0;   // now move it over
    if (!ok) _unlink(totmp.c_str());        // delete so we can try agin; and if this fails, I cannot help it
    return ok;
}

bool convertfromTEXT(const char * from, const char * to, bool make)
{
    if (make && fuptodate(to, from))    // file is up-to-date
        return true;
    string totmp = string(to) + ".tmp$$";
    bool ok = true;
    FILE * fin = NULL;
    FILE * fout = NULL;
    fin = fopen(from, "rb");
    ok = ok && (fin != NULL);
    if (ok)
        fout = fopen(totmp.c_str(), "wb");
    ok = ok && (fout != NULL);

    // skip the header (1024 bytes)
    ok = ok && (fseek(fin, 1024, SEEK_SET) == 0);

    // copy through all characters; expand CR to CRLF
    while (ok)
    {
        int ch = fgetc(fin);
        if (ch == EOF)
        {
            ok = ok && !ferror(fin);
            break;
        }
        else if (ch == 0)       // padding NULs are ignored
            continue;
        else if (ch == 0x10)    // DLE
        {
            int ch2 = fgetc(fin);
            if (ch2 == EOF)
                ok = false;     // not OK to have DLE at the end of the file
            for (int n = 32; n < ch2; n++)
                ok = ok && (fputc(' ', fout) != EOF);
            continue;
        }
        ok = ok && (fputc(ch, fout) != EOF);
        if (ch == '\r')
            ok = ok && (fputc('\n', fout) != EOF);
    }
    ok = ok && (fflush(fout) == 0);

    // done
    if (fin) fclose(fin);
    if (fout) fclose(fout);
    ok = ok && fcpmtime(from, totmp.c_str());    // copy the file date for future up-to-date checks
    ok = ok && (_unlink(to) == 0 || errno == ENOENT);  // now move: first delete existing if any
    ok = ok && rename(totmp.c_str(), to) == 0;   // now move it over
    if (!ok) _unlink(totmp.c_str());        // delete so we can try agin; and if this fails, I cannot help it
    return ok;
}

// TODO: these are copy-pasted from above; needs better code sharing
// 'appendto' may already contain a fake header; it will not be cleared
void converttoTEXT(const wchar_t * from, vector<char> & appendto)
{
    // copy line by line
    string line;
    line.reserve(224);
    for (; ;)
    {
        // get line
        readandconvertline(line, [&]() -> int
        {
            if (*from)
                return *from++;
            else
                return -1;
        });
        if (line.empty())    // indicates end
            break;
        // we need to chunk it into blocks
        // if line does not fit then fill with zeroes first
        size_t charsinblock = appendto.size() % 1024; // filling level of current block
        if (charsinblock + line.size() >= 1024) // compiler detects end of block by a NUL character
            appendto.insert(appendto.end(), 1024 - charsinblock, 0);
        // now either the string fits or we are at a boundary
        // deal with extra-long lines --insert CR like Klebsch/Miller's psystem-fs
        // Note: EDITOR cannot handle lines longer than a much lower limit, so this does not really matter anyway. Maybe 255 is a better max length (matching the Pascal string type).
        while (line.size() > 1023)
        {
            appendto.insert(appendto.end(), line.begin(), line.begin() + 1022);
            appendto.push_back('\r');
            appendto.push_back(0);
            line.erase(line.begin(), line.begin() + 1022);  // and take it out of the string
            // now we are guaranteed again to be on a block boundary
        }
        // write the line --the CR is already part of 'line'
        appendto.insert(appendto.end(), line.begin(), line.end());
    }
}

// TODO: 100% code dup with file versions; needs better sharing
void convertfromTEXT(const std::vector<char> & from, std::vector<wchar_t> & to)
{
    to.reserve (from.size());
    for (size_t k = 0/*assume no header page, e.g. write to char device*/; k < from.size(); k++)
    {
        auto ch = (unsigned char) from[k];  // Pascal chars are BYTEs, unsigned
        if (ch == 0)                        // padding NULs are ignored
            continue;
        else if (ch == 0x10)                // DLE
        {
            if (k+1 < from.size())          // (we ignore an orphaned one at the end)
            {
                auto ch2 = (unsigned char) from[k+1];
                if (ch2 > 32)
                    to.insert(to.end(), ch2-32, ' ');
                k++;
            }
            continue;
        }
        to.push_back(ch);
        if (ch == '\r')
            to.push_back('\n');
    }
}
