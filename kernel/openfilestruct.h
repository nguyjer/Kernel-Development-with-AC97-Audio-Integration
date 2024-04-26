#ifndef _OPENFILESTRUCT_H_
#define _OPENFILESTRUCT_H_

#include "shared.h"
#include "file.h"
#include "ext2.h"

class OpenFileStruct : public File
{
    Shared<Node> node;
    off_t myOffset;

public:
    
    OpenFileStruct(Shared<Node> node) : node(node)
    {
        myOffset = 0;
    }
    bool isU8250() override { return false; }

    bool isFile() override { return node->is_file(); }

    bool isDirectory() override { return node->is_dir(); }

    off_t seek(off_t offset)
    {
        if (offset < 0)
        {
            return -1;
        }
        else if (offset > node->size_in_bytes())
        {
            return -1;
        }
        myOffset = offset;
        return offset;
    }

    off_t size() { return node->size_in_bytes(); }
    ssize_t read(void *buffer, size_t n)
    {
        if (n == 0) {
            return 0;
        }
        int result = node->read_all(myOffset, n, (char*)buffer);
        myOffset += result;
        return result;
    }
    ssize_t write(void *buffer, size_t n)
    {
        Debug::printf("Can't write to files.\n");
        return -1;
    }
    off_t getOffset() { return myOffset; }
};

#endif