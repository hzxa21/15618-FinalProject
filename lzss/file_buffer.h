#pragma once

#include <stdio.h>

#define BUFFER_SIZE 4096000



class FileBuffer {
public:
    FileBuffer(FILE* file) {
        file_ = file;
        offset_ = 0;
        len_ = 0;
    }
  
    // Get one char from buffer. If buffer becomes empty, refill the buffer
    inline int GetChar() {
        // Run out of characters. Read more from
        if (offset_ == len_) {
            len_ = fread(buffer_, sizeof(char), BUFFER_SIZE, file_);
            offset_ = 0;
            if (len_ == 0) {
                // Reach the end of file;
                return EOF;
            }
        }
        return (int)(buffer_[offset_++]);
    }
  
private:
    FILE* file_;
    char buffer_[BUFFER_SIZE];
    unsigned int offset_;
    unsigned int len_;
};