/*********************************************************************\
    bufferedFile.h
    Copyright (c) Thomas Weber. All rights reserved.
    Copyright (C) 2021 The Sanmill developers (see AUTHORS file)
    Licensed under the GPLv3 License.
    https://github.com/madweasel/Muehle
\*********************************************************************/

#ifndef BUFFERED_FILE_H_INCLUDED
#define BUFFERED_FILE_H_INCLUDED

#include <iostream>
#include <string>
#include <windows.h>

using namespace std;

class BufferedFile
{
private:
    // Variables

    // Handle of the file
    HANDLE hFile;

    // number of threads
    unsigned int threadCount;

    // Array of size [threadCount*blockSize] containing the data of the block,
    // where reading is taking place
    unsigned char *readBuf;

    // '' - access by [threadNo*bufSize+position]
    unsigned char *writeBuf;

    // Array of size [threadCount] with pointers to the byte which is currently
    // read
    int64_t *curReadingPointer;
    // ''
    int64_t *curWritingPointer;

    unsigned int *bytesInReadBuf;

    unsigned int *bytesInWriteBuf;

    // size in bytes of a buf
    unsigned int bufSize;

    // size in bytes
    int64_t fileSize;

    CRITICAL_SECTION csIO;

    // Functions
    void writeDataToFile(HANDLE hFile, int64_t offset, unsigned int sizeInBytes,
                         void *pData);
    void readDataFromFile(HANDLE hFile, int64_t offset,
                          unsigned int sizeInBytes, void *pData);

public:
    // Constructor / destructor
    BufferedFile(unsigned int threadCount, unsigned int bufSizeInBytes,
                 const char *fileName);
    ~BufferedFile();

    // Functions
    bool flushBuffers();
    bool writeBytes(unsigned int nBytes, unsigned char *pData);
    bool readBytes(unsigned int nBytes, unsigned char *pData);
    bool writeBytes(unsigned int threadNo, int64_t positionInFile,
                    unsigned int nBytes, unsigned char *pData);
    bool readBytes(unsigned int threadNo, int64_t positionInFile,
                   unsigned int nBytes, unsigned char *pData);
    int64_t getFileSize();
};

#endif // BUFFERED_FILE_H_INCLUDED
