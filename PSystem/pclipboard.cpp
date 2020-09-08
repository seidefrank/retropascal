// clipboard device
// (C) 2013 Frank Seide

#pragma once

#include <collection.h>

#include "pperipherals.h"
#include "await.h"
#include "converttext.h"
#include <vector>

using namespace std;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::DataTransfer;

namespace psystem
{

class clipboardperipheral : public pperipheral
{
    virtual ~clipboardperipheral() { }

    enum iostate { idle, reading, writing };
    iostate state;
    size_t readoffset, writeoffset;
    vector<char> readbuffer, writebuffer;
    Windows::UI::Core::CoreDispatcher^ uithread;

    // block may be -1 (65535)
    virtual IORSLTWD read(void * vp, int boffset, size_t size, int block, size_t mode)
    {
        char * p = boffset + (char*) vp;
        // this is complex
        //  - EDITOR initial read from CLIP: will pass block = -1
        //  - EDITOR copy-from-file from CLIP: will pass block = 2..
        // what we shall return:
        //  - what is read must include the TEXT header
        //  - FBLOCKIO reading from non-block device determines the actual blocks reads by checking trailing zeroes
        //    i.e. trailing zeroes are considered unfulfilled data
        //     - the HEADER page must be returned as 2 blocks, i.e. must have a non-0 in the second half
        //     - EDITOR copy-from-file expects FBLOCKIO to return 2
        //        - we cannot pad with 0 or something; need to patch EDITOR source code
        //        - solution: return IORSLT = INOERROR_return_2 and have emulatesystem() fix it upon proc exit (ugh!)
        //  - FILER is happy with either one, i.e. we can copy from clipboard to a file--yay!
        if (block == 0 || state == writing/*abandoned write*/)
            clear();
        for(;;) switch (state)
        {
        case idle:
            try
            {
                readoffset = 0;
                if (mode & pperipheral::specialmodes::inFBLOCKIO)   // block read: expecting a header page
                {
                    readbuffer.assign(1023, 0); // first page for TEXT file
                    readbuffer.push_back(' ');  // have a non-0 byte in FILLER region to make FBLOCKIO report 2 blocks read (see EDITOR HEADER struct)
                }
                // add actual data
                DataPackageView^ dataPackageView;
                bool hasText;
                dispatchproc(uithread, [&]
                {
                    dataPackageView = Clipboard::GetContent();
                    hasText = dataPackageView->Contains(StandardDataFormats::Text);
                });
                if (hasText)
                {
                    IAsyncOperation<String^>^ contentPromise;
                    dispatchproc(uithread, [&]
                    {
                        contentPromise = dataPackageView->GetTextAsync();
                    });
                    String^ content = await(contentPromise);
                    // convert to block-TEXT format
                    converttoTEXT(content->Data(), readbuffer);
                }
            }
            catch (exception & ex)
            {
                ex;
                return IBADMODE;
            }
            catch (Exception^ ex)
            {
                ex;
                return IBADMODE;
            }
            state = reading;
            break;
        case reading:
            // copy the data back
            if (size > 0)
            {
                if (block >= 0)                 // explicit block size given (e.g. EDITOR copy-from-file)
                {
                    readoffset = block * 512;
                    if (readoffset > readbuffer.size())
                        readoffset = readbuffer.size();     // limit it
                }
                if (readoffset < readbuffer.size())
                {
                    auto copy = min(size, readbuffer.size() - readoffset);
                    memcpy(p, &readbuffer[readoffset], copy);
                    memset(p + copy, 0, size - copy);   // pad rest with 0; FBLOCKIO will check trailing 0 to determine the number of blocks read
                    readoffset += size;
                    // p-code intercept
                    if ((mode & pperipheral::specialmodes::inFBLOCKIO) && size == 1024/*EDITOR copy-from-file*/)
                    {
                        // FBLOCKIO will determine #blocks by trailing zero bytes.
                        // If the second block is empty, it will return the wrong value.
                        // In that case, we fake an eror code, which will cause FBLOCKIO to bypass the RBLOCK and FEOF check.
                        // Then, emulatesystem() picks it up and fakes the expected return value (2).
                        // Determine the actual number of 0-bytes. There may be a >512 byte hole in the readbuffer if a line >512 chars is forced to snap to the next buffer boundary.
                        size_t nonzerobytes = copy;
                        while (nonzerobytes > 0 && p[nonzerobytes-1] == 0)
                            nonzerobytes--;
                        if (nonzerobytes < 512)
                            return INOERROR_return_2;  // special error: FBLOCKIO bypasses RBLOCK and FEOF check, instead emulatesystem() fakes the return value (2)
                    }
                    // end p-code intercept
                    return INOERROR;
                }
                else
                {
                    // back to idle once all is read
                    memset(p, 0, size); // fill buffer with 0, FBLOCKIO will check trailing 0s and report 0 blocks read
                    state = idle;
                    return INOERROR;
                }
            }
        }
    }

    virtual IORSLTWD write(const void * vp, int boffset, size_t size, int block, size_t mode)
    {
        char * p = boffset + (char*) vp;
        if (state != writing/*abandoned read*/)
            clear();
        state = writing;
        try
        {
            writebuffer.insert(writebuffer.end(), p, p + size);
        }
        catch (exception & ex)
        {
            ex;
            return ISTRGOVFL;
        }
        // finalize() will actually send it to the WinRT clipboard, to be called by FCLOSE
        return INOERROR;
    }

    // Note: It is not clear whether we will receive the TEXT header block or not.
    // FILER knows not to copy it, and our patch to EDITOR that writes to CLIP:
    // knows that as well. But e.g. saving the file to CLIP: does include it.
    virtual IORSLTWD finalize()   // commit write buffer: means we are done
    {
        if (state == writing && writebuffer.size() > 0)
        {
            // convert buffer to Windows format
            vector<wchar_t> text;
            try
            {
                convertfromTEXT(writebuffer, text);
            }
            catch (exception & ex)
            {
                ex;
                return ISTRGOVFL;
            }
            // send to WinRT
            if (!text.empty()) try
            {
                auto dataPackage = ref new DataPackage();
                dataPackage->RequestedOperation = DataPackageOperation::Copy;
                dataPackage->SetText(ref new String(text.data(), text.size()));
                dispatchproc(uithread, [&]
                {
                    Clipboard::SetContent(dataPackage);
                });
            }
            catch (Exception^ ex)
            {
                ex;
                return IBADMODE;
            }
        }
        clear();
        return INOERROR;
    }

    virtual IORSLTWD stat(WORD & res)
    {
        return INOERROR;
    }

    virtual IORSLTWD clear()
    {
        state = idle;
        readoffset = writeoffset = 0;
        readbuffer.clear(); writebuffer.clear();
        return INOERROR;
    }
public:
    clipboardperipheral()
    {
        uithread = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;
        clear();
    }
};

pperipheral * createclipboardperipheral() { return new clipboardperipheral(); }

};
