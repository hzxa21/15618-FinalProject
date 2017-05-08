/***************************************************************************
*                     Sample Program Using LZSS Library
*
*   File    : sample.c
*   Purpose : Demonstrate usage of LZSS library
*   Author  : Michael Dipperstein
*   Date    : February 21, 2004
*
****************************************************************************
*
* SAMPLE: Sample usage of LZSS Library
* Copyright (C) 2004, 2006, 2007, 2014 by
* Michael Dipperstein (mdipper@alumni.engr.ucsb.edu)
*
* This file is part of the lzss library.
*
* The lzss library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 3 of the
* License, or (at your option) any later version.
*
* The lzss library is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
* General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <exception>
#include "lzss.h"
#include "optlist.h"

using std::string;
using std::exception;

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/
typedef enum
{
    ENCODE,
    DECODE,
    TEST
} modes_t;


FILE* OpenFile(const char* file_name, const char* mode);

/****************************************************************************
*   Function   : main
*   Description: This is the main function for this program, it validates
*                the command line input and, if valid, it will either
*                encode a file using the LZSS algorithm or decode a
*                file encoded with the LZSS algorithm.
*   Parameters : argc - number of parameters
*                argv - parameter list
*   Effects    : Encodes/Decodes input file
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int main(int argc, char *argv[])
{
    option_t *optList;
    option_t *thisOpt;
    FILE *fpIn = NULL;             /* pointer to open input file */
    FILE *fpOut = NULL;            /* pointer to open output file */
    string infile_name;
    string outfile_name;
    modes_t mode = TEST;

    /* parse command line */
    optList = GetOptList(argc, argv, "cdi:o:h?");
    thisOpt = optList;

    while (thisOpt != NULL)
    {
        switch(thisOpt->option)
        {
            case 'c':       /* compression mode */
                mode = ENCODE;
                break;

            case 'd':       /* decompression mode */
                mode = DECODE;
                break;
            case 't':
                mode = TEST;
                break;
            case 'i':       /* input file name */
                infile_name = thisOpt->argument;
                break;

            case 'o':       /* output file name */
                outfile_name = thisOpt->argument;
                break;

            case 'h':
            case '?':
                printf("Usage: %s <options>\n\n", FindFileName(argv[0]));
                printf("options:\n");
                printf("  -t : Test correctness and do performance test.\n");
                printf("  -c : Encode input file to output file.\n");
                printf("  -d : Decode input file to output file.\n");
                printf("  -i <filename> : Name of input file.\n");
                printf("  -o <filename> : Name of output file.\n");
                printf("  -h | ?  : Print out command line options.\n\n");
                printf("Default: %s -c -i stdin -o stdout\n",
                    FindFileName(argv[0]));

                FreeOptList(optList);
                return 0;
        }

        optList = thisOpt->next;
        free(thisOpt);
        thisOpt = optList;
    }

    // Initialize the timer
    memset(encoding_times, 0, 3 * sizeof(double));
  
    /* we have valid parameters encode or decode */
    if (mode == TEST) {
        // Test mode
        // Step 1: Compressed the input file
        fpIn = fopen(infile_name.c_str(), "rb");
        fpOut = OpenFile("compressed", "wb");
        EncodeLZSS(fpIn, fpOut);
        fclose(fpIn);
        fclose(fpOut);
        
        // Step 2: Decompressed the intermediate file
        fpIn = OpenFile("compressed", "rb");
        fpOut = OpenFile("decompressed", "wb");
        DecodeLZSS(fpIn, fpOut);
        fclose(fpIn);
        fclose(fpOut);
        
        // Step 3: Test the correctness
        string cmd = string("diff decompressed ") + infile_name;
        int ret_code = system(cmd.c_str());
        if (ret_code == 0)
            fprintf(stdout, "Compression result is correct!\n");
        else
            fprintf(stdout, "Error: Compression result is not correct!\n");
        
        // Step 4: Print out statistics
        // Encoding is divided into three main steps. First the lookahead buffer
        // is compared against the sliding window. Secondly, the encoding offset +
        // length is written to the file. Thirdly, the matching bytes are moved from
        // lookahead buffer to the sliding window. Also, more characters are read
        // from disk to fill the lookahead buffer
        fprintf(stdout, "********* Encoding Statistics **********\n");
        fprintf(stdout, "Step 1 (find string match) takes %f seconds\n",
                encoding_times[0]);
        fprintf(stdout, "Step 2 (write encoded str to file) takes %f seconds\n",
                encoding_times[1]);
        fprintf(stdout, "Step 3 (Update sliding window and read more chars) takes"
                " %f seconds\n", encoding_times[2]);
    } else {
        /* use stdin/out if no files are provided */
        fpIn = infile_name.empty() ? stdin : fopen(infile_name.c_str(), "rb");
        fpOut = outfile_name.empty() ? stdout : fopen(outfile_name.c_str(), "wb");
        
        if (mode == ENCODE) {
            EncodeLZSS(fpIn, fpOut);
        } else if (mode == DECODE) {
            DecodeLZSS(fpIn, fpOut);
        }
        
        fclose(fpIn);
        fclose(fpOut);
    }

    return 0;
}

FILE* OpenFile(const char* file_name, const char* mode) {
    FILE* fp = fopen(file_name, mode);
    if (fp == NULL) {
        perror("Error opening file");
        throw exception();
    }
    return fp;
}
