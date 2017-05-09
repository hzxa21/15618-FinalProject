#pragma once

#include <stdio.h>

#define BUFFER_SIZE 40960000



class FileBuffer {
public:
    FileBuffer(FILE* file) {
        file_ = file;
        offset_ = 0;
        len_ = 0;
        buffer_ = new unsigned char[BUFFER_SIZE];
    }
    
    ~FileBuffer() {
        delete[] buffer_;
    }
  
    // Get one char from buffer. If buffer becomes empty, refill the buffer
    inline int GetChar() {
        return getc(file_);
        // Run out of characters. Read more from
        if (offset_ == len_) {
            len_ = fread(buffer_, sizeof(unsigned char), BUFFER_SIZE, file_);
            offset_ = 0;
            if (len_ == 0) {
                // Reach the end of file;
                return EOF;
            }
        }
        return buffer_[offset_++];
    }
  
private:
    FILE* file_;
    unsigned char* buffer_;
    unsigned int offset_;
    unsigned int len_;
};