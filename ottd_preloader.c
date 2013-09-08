#include <lzma.h>
#include <zlib.h>
#include <lzo/lzo1x.h>
#include "ottd.h"

#define Vprintf(...) do{if(verbose){printf(__VA_ARGS__);}}while(0);
#define Veprintf(...) do{if(verbose){fprintf(stderr, __VA_ARGS__);}}while(0);
#define eprintf(...) fprintf(stderr, __VA_ARGS__);

FILE* ottd_tmpfile()
{
    return tmpfile();
}

FILE* ottd_decompress_none(FILE *fp, uint16_t version, int verbose)
{
    FILE *tf = ottd_tmpfile();
    if (tf == NULL) {
        Veprintf("tmpfile: %s", strerror(errno));
        return NULL;
    }
    
    // copy whole file starting at 8
    fseek(fp, 8, SEEK_SET);
    size_t bufsz = 64 * 1024, bufrd;
    uint8_t *buf = malloc(bufsz);
    while(!feof(fp)) {
        bufrd = fread(buf, 1, bufsz, fp);
        fwrite(buf, 1, bufrd, tf);
    }
    free(buf);
    
    rewind(tf);
    return tf;
}

FILE* ottd_decompress_lzo(FILE *fp, uint16_t version, int verbose)
{
    Vprintf("decompressing lzo...\n");
    
    FILE *tf = ottd_tmpfile();
    if (tf == NULL) {
        Veprintf("tmpfile: %s", strerror(errno));
        return NULL;
    }
    
    // initialize decoder
    if (lzo_init() != LZO_E_OK) {
        Veprintf("could not initialize decompressor\n");
        fclose(tf);
        return NULL;
    }
    
    // buffers
    size_t LZO_BUFFER_SIZE = 8192;
    uint8_t buf[LZO_BUFFER_SIZE + 64];
    do {
        // openttd told me to do this
        uint8_t out[LZO_BUFFER_SIZE + LZO_BUFFER_SIZE / 16 + 64 + 3 + 8];
        uint32_t tmp[2], size;
        lzo_uint len;
        
        // read header
        size_t br = fread(tmp, 1, 8, fp);
        if (br == 0 && feof(fp)) break;
        if (br != 8) goto read_error;
        
        // check size
        ((uint32_t*)out)[0] = size = tmp[1];
        if (version != 0) {
            tmp[0] = ntohl(tmp[0]);
            size = ntohl(size);
        }
        if (size >= sizeof(out)) {
            eprintf("corrupt lzo block: inconsistent size\n");
            fclose(tf);
            return NULL;
        }
        
        // read block
        if (fread(out+4, 1, size, fp) != size) goto read_error;
        
        // verify checksum
        if (tmp[0] != lzo_adler32(0, out, size + 4)) {
            eprintf("corrupt lzo block: bad checksum\n");
            fclose(tf);
            return NULL;
        }
        
        // decompress
        lzo1x_decompress(out + 4, size, buf, &len, NULL);
        
        // write
        fwrite(buf, 1, len, tf);
    } while(1);
    
    // end
    rewind(tf);
    return tf;

read_error:
    eprintf("fread: %s\n", strerror(errno));
    fclose(tf);
    return NULL;
}

FILE* ottd_decompress_zlib(FILE *fp, uint16_t version, int verbose)
{
    Vprintf("decompressing zlib...\n");
    
    FILE *tf = ottd_tmpfile();
    if (tf == NULL) {
        Veprintf("tmpfile: %s", strerror(errno));
        return NULL;
    }
    
    // init decoder
    z_stream z = {
        .zalloc = NULL,
        .zfree = NULL,
        .opaque = NULL,
        .avail_in = 0,
        .next_in = NULL
    };
    if (inflateInit(&z) != Z_OK) {
        Veprintf("could not initialize decompressor\n");
        fclose(tf);
        return NULL;
    }
    
    // buffers
    size_t bufsz = 64 * 1024, bufrd;
    uint8_t *rbuf = malloc(bufsz);
    uint8_t *wbuf = malloc(bufsz);
    
    do {
        // read from file
        if (z.avail_in == 0) {
            if ((bufrd = fread(rbuf, 1, bufsz, fp)) <= 0) {
                eprintf("fread: %s\n", strerror(errno));
                free(rbuf); free(wbuf);
                fclose(tf);
                inflateEnd(&z);
                return NULL;
            }
            z.next_in = rbuf;
            z.avail_in = (uInt)bufrd;
        }
        
        // decode
        z.next_out = wbuf;
        z.avail_out = (uInt)bufsz;
        int r = inflate(&z, Z_NO_FLUSH);
        
        // write decoded
        if ((bufsz - z.avail_out) > 0) {
            fwrite(wbuf, 1, bufsz - z.avail_out, tf);
        }
        
        // end?
        if (r == Z_STREAM_END) break;
        if (r != Z_OK) {
            eprintf("zlib not ok\n");
            free(rbuf); free(wbuf);
            fclose(tf);
            inflateEnd(&z);
            return NULL;
        }
    } while(1);
    
    // end
    inflateEnd(&z);
    free(rbuf); free(wbuf);
    rewind(tf);
    return tf;
}

FILE* ottd_decompress_lzma(FILE *fp, uint16_t version, int verbose)
{
    Vprintf("decompressing lzma...\n");
    
    FILE *tf = ottd_tmpfile();
    if (tf == NULL) {
        Veprintf("tmpfile: %s", strerror(errno));
        return NULL;
    }
    
    // init decoder
    lzma_stream lzma = LZMA_STREAM_INIT;
    if (lzma_auto_decoder(&lzma, 1 << 28, 0) != LZMA_OK) {
        Veprintf("could not initialize decompressor\n");
        fclose(tf);
        return NULL;
    }
    
    // buffers
    size_t bufsz = 64 * 1024, bufrd;
    uint8_t *rbuf = malloc(bufsz);
    uint8_t *wbuf = malloc(bufsz);
    
    do {
        // read from file
        if (lzma.avail_in == 0) {
            if ((bufrd = fread(rbuf, 1, bufsz, fp)) <= 0) {
                eprintf("fread: %s\n", strerror(errno));
                free(rbuf); free(wbuf);
                fclose(tf);
                lzma_end(&lzma);
                return NULL;
            }
            lzma.next_in = rbuf;
            lzma.avail_in = bufrd;
        }
        
        // decode
        lzma.next_out = wbuf;
        lzma.avail_out = bufsz;
        lzma_ret r = lzma_code(&lzma, LZMA_RUN);
        
        // write decoded
        if ((bufsz - lzma.avail_out) > 0) {
            fwrite(wbuf, 1, bufsz - lzma.avail_out, tf);
        }
        
        // end?
        if (r == LZMA_STREAM_END) break;
        if (r != LZMA_OK) {
            eprintf("lzma not ok\n");
            free(rbuf); free(wbuf);
            fclose(tf);
            lzma_end(&lzma);
            return NULL;
        }
    } while(1);
    
    // end
    lzma_end(&lzma);
    free(rbuf); free(wbuf);
    rewind(tf);
    return tf;
}
