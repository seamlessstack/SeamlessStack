#include <stdio.h>
#include <stdlib.h>
#include "lz4hc.h"

#define LZ4S_BLOCKSIZEID_DEFAULT 7
#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)
#define MIN_STREAM_BUFSIZE (1 MB + 64 KB)
#define CACHELINE 64
#define LZ4S_CHECKSUM_SEED 0
#define LZ4S_MAGICNUMBER 0x184D2204
#define _1BIT 0x01
#define _2BITS 0x03
#define _3BITS 0x07
#define _4BITS 0xf
#define _8BITS 0xff
#define LZ4S_EOS 0
static const int one = 1;
#define swap32 __builtin_bswap32
#define CPU_LITTLE_ENDIAN (*(char*)(&one))
#define LITTLE_ENDIAN_32(i) (CPU_LITTLE_ENDIAN?(i):swap32(i))

static int
LZ4S_GetBlockSize_FromBlockId(int id)
{
	return (1 << (8 + (2 * id)));
}

static unsigned int
LZ4S_GetCheckBits_FromXXH(unsigned int xxh)
{
	return (xxh >> 8) & _8BITS;
}

int
compress_file(char *input_filename, char *output_filename)
{

    void* (*initFunction)       (const char*);
    int   (*compressionFunction)(void*, const char*, char*, int, int);
    char* (*translateFunction)  (void*);
    int   (*freeFunction)       (void*);
    void* ctx;
    unsigned long long filesize = 0;
    unsigned long long compressedfilesize = 0;
    unsigned int checkbits;
    char* in_buff, *in_start, *in_end;
    char* out_buff;
    FILE* finput;
    FILE* foutput;
    clock_t start, end;
    unsigned int blockSize, inputBufferSize;
    size_t sizeCheck, header_size;
    void* streamChecksumState=NULL;


	initFunction = LZ4_createHC;
	compressionFunction = LZ4_compressHC_limitedOutput_continue;
	translateFunction = LZ4_slideInputBufferHC;
	freeFunction = LZ4_freeHC;
	// Open files
	finput = fopen(input_filename, "r");
	if (NULL == finput) {
		fprintf(stderr, "%s: Failed to open file %s \n", __FUNCTION__,
						input_filename);
		return -1;
	}
	foutput = fopen(output_filename, "w+");
	if (NULL == foutput) {
		fprintf(stderr, "%s: Failed to open file %s \n", __FUNCTION__,
						output_filename);
		return -1;
	}

    blockSize = LZ4S_GetBlockSize_FromBlockId (LZ4S_BLOCKSIZEID_DEFAULT);

    // Allocate Memory
    inputBufferSize = blockSize + 64 KB;
    if (inputBufferSize < MIN_STREAM_BUFSIZE)
		inputBufferSize = MIN_STREAM_BUFSIZE;
    in_buff  = (char*)malloc(inputBufferSize);
    out_buff = (char*)malloc(blockSize+CACHELINE);
    if (!in_buff || !out_buff) {
		fprintf(stderr, "%s: Allocation error : not enough memory\n",
						__FUNCTION__);
		return -1;
	}
    in_start = in_buff; in_end = in_buff + inputBufferSize;
	streamChecksumState = XXH32_init(LZ4S_CHECKSUM_SEED);
    ctx = initFunction(in_buff);

    // Write Archive Header
    *(unsigned int*)out_buff = LITTLE_ENDIAN_32(LZ4S_MAGICNUMBER);
    *(out_buff+4)  = (1 & _2BITS) << 6 ;
    *(out_buff+4) |= (1 & _1BIT) << 5; // blockIndependence
    *(out_buff+4) |= (1 & _1BIT) << 4; // blockChecksum
    *(out_buff+4) |= (1 & _1BIT) << 2; // streamChecksum
    *(out_buff+5)  = (char)((LZ4S_BLOCKSIZEID_DEFAULT & _3BITS) << 4);
    checkbits = XXH32((out_buff+4), 2, LZ4S_CHECKSUM_SEED);
    checkbits = LZ4S_GetCheckBits_FromXXH(checkbits);
    *(out_buff+6)  = (unsigned char) checkbits;
    header_size = 7;
    sizeCheck = fwrite(out_buff, 1, header_size, foutput);
    if (sizeCheck != header_size) {
		fprintf(stderr, "%s: Write error : cannot write header \n",
						__FUNCTION__);
		return -1;
	}
    compressedfilesize += header_size;

    // Main Loop
    while (1) {
        unsigned int outSize;
        unsigned int inSize;

        // Read Block
        if ((in_start+blockSize) > in_end)
			in_start = translateFunction(ctx);
        inSize = (unsigned int) fread(in_start, (size_t)1,
						(size_t)blockSize, finput);
        if( inSize==0 )
			break;   // No more input : end of compression
        filesize += inSize;
        XXH32_update(streamChecksumState, in_start, inSize);

        // Compress Block
        outSize = compressionFunction(ctx, in_start, out_buff+4,
						inSize, inSize-1);
        if (outSize > 0)
			compressedfilesize += outSize+4;
		else
			compressedfilesize += inSize+4;
        compressedfilesize+=4;

        // Write Block
        if (outSize > 0) {
            int sizeToWrite;
            * (unsigned int*) out_buff = LITTLE_ENDIAN_32(outSize);
			unsigned int checksum = XXH32(out_buff+4, outSize,
							LZ4S_CHECKSUM_SEED);
			* (unsigned int*) (out_buff+4+outSize) = LITTLE_ENDIAN_32(checksum);
            sizeToWrite = 4 + outSize + (4*1);
            sizeCheck = fwrite(out_buff, 1, sizeToWrite, foutput);
            if (sizeCheck!=(size_t)(sizeToWrite)) {
				fprintf(stderr, "%s: Write error : cannot write compressed "
							"block \n", __FUNCTION__);
				return -1;
			}
        } else  {
			// Copy Original
			// Add uncompressed flag
            * (unsigned int*) out_buff = LITTLE_ENDIAN_32(inSize|0x80000000);
            sizeCheck = fwrite(out_buff, 1, 4, foutput);
            if (sizeCheck!=(size_t)(4)) {
				fprintf(stderr, "%s: Write error : cannot write block header\n",
							__FUNCTION__);
				return -1;
			}
            sizeCheck = fwrite(in_start, 1, inSize, foutput);
            if (sizeCheck!=(size_t)(inSize)) {
				fprintf(stderr, "%s: Write error : cannot write block \n",
							__FUNCTION__);
			}
            {
                unsigned int checksum =
						XXH32(in_start, inSize, LZ4S_CHECKSUM_SEED);
                * (unsigned int*) out_buff = LITTLE_ENDIAN_32(checksum);
                sizeCheck = fwrite(out_buff, 1, 4, foutput);
                if (sizeCheck!=(size_t)(4)) {
					fprintf(stderr, "%s: Write error : cannot write block "
								"checksum \n", __FUNCTION__);
					return -1;
				}
            }
        }
        in_start += inSize;
    }

    // End of Stream mark
    * (unsigned int*)out_buff = LZ4S_EOS;
    sizeCheck = fwrite(out_buff, 1, 4, foutput);
    if (sizeCheck!=(size_t)(4)) {
		fprintf(stderr, "%s: Write error : cannot write end of stream \n",
					__FUNCTION__);
		return -1;
	}
    compressedfilesize += 4;
    {
        unsigned int checksum = XXH32_digest(streamChecksumState);
        * (unsigned int*) out_buff = LITTLE_ENDIAN_32(checksum);
        sizeCheck = fwrite(out_buff, 1, 4, foutput);
        if (sizeCheck!=(size_t)(4)) {
			fprintf(stderr, "%s: Write error : cannot write stream checksum \n",
						__FUNCTION__);
			return -1;
		}
        compressedfilesize += 4;
    }

    // Status
    fprintf(stderr, "Compressed %llu bytes into %llu bytes ==> %.2f%%\n",
        (unsigned long long) filesize,
		(unsigned long long) compressedfilesize,
		(double)compressedfilesize/filesize*100);

    // Close & Free
    freeFunction(ctx);
    free(in_buff);
    free(out_buff);
    fclose(finput);
    fclose(foutput);

    return 0;
}


int main(void)
{
	compress_file("/home/anand/input_file", "/tmp/output");

	return 0;
}
