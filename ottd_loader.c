#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#ifdef __WIN32__
#include <windows.h>
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
#include <stdint.h>
#include "ottd.h"

#define TYPECHARS(t) ((t) >> 24) & 0xFF, ((t) >> 16) & 0xFF, ((t) >> 8) & 0xFF, (t) & 0xFF
#define lengthof(x) (sizeof(x) / sizeof(x[0]))
#define SL_MAX_VERSION 255

#define Vprintf(...) do{if(verbose){printf(__VA_ARGS__);}}while(0);
#define Veprintf(...) do{if(verbose){fprintf(stderr, __VA_ARGS__);}}while(0);
#define eprintf(...) fprintf(stderr, __VA_ARGS__);

#define ORIGINAL_BASE_YEAR 1920

// reading
void* ottd_read(FILE *fp, size_t size);
void ottd_skip(FILE *fp, size_t len);
uint8_t ottd_read_u8(FILE *fp);
uint16_t ottd_read_u16(FILE *fp);
uint32_t ottd_read_u32(FILE *fp);
uint64_t ottd_read_u64(FILE *fp);
uint32_t ottd_read_sg(FILE *fp);
uint32_t ottd_read_riff_length(FILE *fp);
char *ottd_read_str(FILE *fp);

// decompression
FILE* ottd_decompress_none(FILE *fp, uint16_t version, int verbose);
FILE* ottd_decompress_lzo(FILE *fp, uint16_t version, int verbose);
FILE* ottd_decompress_zlib(FILE *fp, uint16_t version, int verbose);
FILE* ottd_decompress_lzma(FILE *fp, uint16_t version, int verbose);

// chunky stuff
typedef struct ChunkProc {
    uint32_t type;
    int(*proc)(FILE*,int,ottd_t*);
} ChunkProc;

int ottd_skip_riff(FILE *fp, int verbose, ottd_t*save);
int ottd_skip_array(FILE *fp, int verbose, ottd_t*save);
#define ottd_skip_sparse ottd_skip_array
int ottd_read_MAPS(FILE *fp, int verbose, ottd_t*save);
int ottd_read_MAPT(FILE *fp, int verbose, ottd_t*save);
int ottd_read_MAPO(FILE *fp, int verbose, ottd_t *save);
int ottd_read_DATE(FILE *fp, int verbose, ottd_t *save);
int ottd_read_PATS(FILE *fp, int verbose, ottd_t *save);
int ottd_read_PLYR(FILE *fp, int verbose, ottd_t *save);

static ChunkProc ChunkProcs[] = {
    {'AIPL', ottd_skip_array},
    {'ANIT', ottd_skip_riff},
    {'APID', ottd_skip_array},
    {'ATID', ottd_skip_array},
    {'BKOR', ottd_skip_array},
    {'CAPA', ottd_skip_array},
    {'CAPR', ottd_skip_riff},
    {'CAPY', ottd_skip_array},
    {'CHKP', ottd_skip_array},
    {'CHTS', ottd_skip_riff},
    {'CITY', ottd_skip_array},
    {'CMDL', ottd_skip_array},
    {'CMPU', ottd_skip_array},
    {'DATE', ottd_read_DATE},
    {'DEPT', ottd_skip_array},
    {'ECMY', ottd_skip_riff},
    {'EIDS', ottd_skip_array},
    {'ENGN', ottd_skip_array},
    {'ENGS', ottd_skip_riff},
    {'ERNW', ottd_skip_array},
    {'GLOG', ottd_skip_riff},
    {'GOAL', ottd_skip_array},
    {'GRPS', ottd_skip_array},
    {'GSDT', ottd_skip_array},
    {'GSTR', ottd_skip_array},
    {'HIDS', ottd_skip_array},
    {'IBLD', ottd_skip_riff},
    {'IIDS', ottd_skip_array},
    {'INDY', ottd_skip_array},
    {'ITBL', ottd_skip_array},
    {'LGRJ', ottd_skip_array},
    {'LGRP', ottd_skip_array},
    {'LGRS', ottd_skip_array},
    {'M3HI', ottd_skip_riff},
    {'M3LO', ottd_skip_riff},
    {'MAP2', ottd_skip_riff},
    {'MAP5', ottd_skip_riff},
    {'MAP7', ottd_skip_riff},
    {'MAPE', ottd_skip_riff},
    {'MAPH', ottd_skip_riff},
    {'MAPO', ottd_read_MAPO},
    {'MAPS', ottd_read_MAPS},
    {'MAPT', ottd_read_MAPT},
    {'NAME', ottd_skip_array},
    {'NGRF', ottd_skip_array},
    {'OBID', ottd_skip_array},
    {'OBJS', ottd_skip_array},
    {'OPTS', ottd_skip_riff},
    {'ORDL', ottd_skip_array},
    {'ORDR', ottd_skip_array},
    {'PATS', ottd_read_PATS},
    {'PLYR', ottd_read_PLYR},
    {'PRIC', ottd_skip_riff},
    {'PSAC', ottd_skip_array},
    {'RAIL', ottd_skip_array},
    {'ROAD', ottd_skip_array},
    {'SIGN', ottd_skip_array},
    {'STNN', ottd_skip_array},
    {'STNS', ottd_skip_array},
    {'STPA', ottd_skip_array},
    {'STPE', ottd_skip_array},
    {'SUBS', ottd_skip_array},
    {'TIDS', ottd_skip_array},
    {'VEHS', ottd_skip_sparse},
    {'VIEW', ottd_skip_riff}
};

int ottd_chunkproc_cmp(const void *key, const void *val)
{
    int32_t tKey = *(int32_t*)key;
    int32_t tVal = (int32_t)((ChunkProc*)val)->type;
    return tKey-tVal;
}

ottd_t* ottd_load(const char *path, int verbose)
{
    ottd_t *game = calloc(1, sizeof(ottd_t));
    if (game == NULL) return NULL;
    
    FILE *savefp = fopen(path, "rb");
    FILE *fp = NULL;
    if (savefp == NULL) return NULL;
    
    // read header
    uint8_t header[8];
    uint32_t format, version;
    if (fread(header, 8, 1, savefp) != 1) goto fail;
    format = ntohl(*(uint32_t*)header);
    version = ntohl(*(uint32_t*)(header+4));
    version >>= 16;
    game->version = version;

    Vprintf("version: %d\n", version);

    // decompress
    FILE*(*dcmp_fcn)(FILE*,uint16_t,int) = NULL;
    switch(format) {
        case 'OTTD': dcmp_fcn = ottd_decompress_lzo; break;
        case 'OTTN': dcmp_fcn = ottd_decompress_none; break;
        case 'OTTZ': dcmp_fcn = ottd_decompress_zlib; break;
        case 'OTTX': dcmp_fcn = ottd_decompress_lzma; break;
        default:
            // unsupported format
            errno = EINVAL;
            Veprintf("unsupported format: %c%c%c%c\n", header[0], header[1], header[2], header[3]);
            goto fail;
    }
    fp = dcmp_fcn(savefp, version, verbose);
    fclose(savefp); savefp = NULL;

    // load chunks
    uint32_t chunkType;
    do {
        chunkType = ottd_read_u32(fp);
        if (chunkType == 0) break;
        Vprintf("read chunk %c%c%c%c...\n", TYPECHARS(chunkType));
        const ChunkProc *proc = bsearch(&chunkType, ChunkProcs, lengthof(ChunkProcs), sizeof(ChunkProcs[0]), ottd_chunkproc_cmp);
        if (proc == NULL) {
            char chunkTypeStr[5];
            *(uint32_t*)chunkTypeStr = htonl(chunkType);
            eprintf("don't know what to do with chunkType %4s\n", chunkTypeStr);
        } else {
            proc->proc(fp, verbose, game);
        }

    } while(1);

    // this is the end
    fclose(fp); fp = NULL;
    return game;

fail:
    ottd_free(game);
    if (savefp) fclose(savefp);
    if (fp) fclose(fp);
    return NULL;
}

void ottd_free(ottd_t *save)
{
    if (save == NULL) return;
    
    // free companies
    for(int i=0; i < lengthof(save->company); i++) {
        free(save->company[i].name);
        free(save->company[i].manager);
    }
    
    // free tiles
    if (save->tile) {
        free(save->tile[0]);
        free(save->tile);
    }
    
    free(save);
}

#pragma mark - Low-level reading

void* ottd_read(FILE *fp, size_t size)
{
    void *mem = malloc(size);
    if (mem == NULL) return NULL;
    if (fread(mem, 1, size, fp) != size) {
        free(mem);
        return NULL;
    }
    return mem;
}

void ottd_skip(FILE *fp, size_t len)
{
    fseek(fp, len, SEEK_CUR);
}

uint8_t ottd_read_u8(FILE *fp)
{
    uint8_t b;
    if (fread(&b, 1, 1, fp) != 1) return 0;
    return b;
}

uint16_t ottd_read_u16(FILE *fp)
{
    uint16_t val;
    if (fread(&val, 1, sizeof val, fp) != sizeof val) return 0;
    return ntohs(val);
}

uint32_t ottd_read_u32(FILE *fp)
{
    uint32_t val;
    if (fread(&val, 1, sizeof val, fp) != sizeof val) return 0;
    return ntohl(val);
}

uint64_t ottd_read_u64(FILE *fp)
{
    uint8_t b[8];
    if (fread(b, 1, 8, fp) != 8) return 0;
    return ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) | ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
        ((uint64_t)b[4] << 24) |  ((uint64_t)b[5] << 16) |  ((uint64_t)b[6] << 8) | (uint64_t)b[7];
}

char *ottd_read_str(FILE *fp)
{
    uint32_t len = ottd_read_sg(fp);
    if (len == 0) return NULL;
    char *str = malloc(len+1);
    str[len] = '\0';
    if (fread(str, 1, len, fp) != len) {
        free(str);
        return NULL;
    }
    return str;
}

/* 
 * 0xxxxxxx
 * 10xxxxxx xxxxxxxx
 * 110xxxxx xxxxxxxx xxxxxxxx
 * 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
 */
uint32_t ottd_read_sg(FILE *fp)
{
    int btr = 0;
    uint8_t b;
    uint32_t res;
    
    // read first byte
    if (fread(&b, 1, 1, fp) != 1) return 0;
    
    // what do?
    if ((b & 0x80) == 0) {
        // 1 byte
        res = b;
        btr = 0;
    } else if ((b & 0xC0) == 0x80) {
        // 2 bytes
        res = (b & 0x3F);
        btr = 1;
    } else if ((b & 0xE0) == 0xC0) {
        // 3 bytes
        res = (b & 0x1F);
        btr = 2;
    } else if ((b & 0xF0) == 0xE0) {
        // 4 bytes
        res = (b & 0x0F);
        btr = 3;
    }
    
    // read the other bytes
    while(btr--) {
        res <<= 8;
        if (fread(&b, 1, 1, fp) != 1) return 0;
        res |= b;
    }
    
    return res;
}

uint32_t ottd_read_riff_length(FILE *fp)
{
    uint32_t len = ottd_read_u32(fp);
    if (len == 0) return -1;
    len = (len & 0xFFFFFF) | ((len >> 24) << 28);
    return len;
}

size_t ottd_sg_len(uint32_t sg)
{
    if (sg <= 0x7F) return 1;
    else if (sg <= 0x3FFF) return 2;
    else if (sg <= 0x1FFFFF) return 3;
    else if (sg <= 0x0FFFFFFF) return 4;
    return 0;
}

#pragma mark - Chunk Reading

int ottd_skip_riff(FILE *fp, int verbose, ottd_t *save)
{
    uint32_t len = ottd_read_riff_length(fp);
    if (len == 0) return -1;
    fseek(fp, len, SEEK_CUR);
    return 0;
}

int ottd_skip_array(FILE *fp, int verbose, ottd_t *save)
{
    uint8_t mark = ottd_read_u8(fp);
    if (mark != 1 && mark != 2) return -1; // array or sparse array marker
    
    // elements
    uint32_t len;
    while((len = ottd_read_sg(fp))) {
        fseek(fp, len - 1, SEEK_CUR);
    }
    
    return 0;
}

int ottd_read_MAPS(FILE *fp, int verbose, ottd_t *save)
{
    uint32_t len = ottd_read_riff_length(fp);
    if (len < 8) return -1;
    save->mapSize.x = ottd_read_u32(fp);
    save->mapSize.y = ottd_read_u32(fp);
    Vprintf("Map size: %ux%u\n", save->mapSize.x, save->mapSize.y);
    
    // allocate tile types
    save->tile = malloc(sizeof(ottd_tile_t*)*save->mapSize.y);
    ottd_tile_t *ttmp = malloc(sizeof(ottd_tile_t)*save->mapSize.x*save->mapSize.y);
    for(int i=0; i < save->mapSize.y; i++) {
        save->tile[i] = &ttmp[save->mapSize.x*i];
    }
    
    if (len > 8) fseek(fp, len-8, SEEK_CUR);
    return 0;
}

int ottd_read_MAPT(FILE *fp, int verbose, ottd_t *save)
{
    uint32_t len = ottd_read_riff_length(fp);
    if (len != save->mapSize.x * save->mapSize.y) {
        Veprintf("MAPT size doesn't match map size\n");
        return -1;
    }
    
    // read tiles
    uint8_t *tiles = ottd_read(fp, len);
    for(int i=0; i < len; i++) {
        save->tile[0][i].type = (tiles[i] & 0xF0) >> 4;
        save->tile[0][i].height = tiles[i] & 0x0F;
    }
    
    free(tiles);
    return len;
}

int ottd_read_MAPO(FILE *fp, int verbose, ottd_t *save)
{
    uint32_t len = ottd_read_riff_length(fp);
    if (len != save->mapSize.x * save->mapSize.y) {
        Veprintf("MAPT size doesn't match map size\n");
        return -1;
    }
    
    // read tiles
    uint8_t *tiles = ottd_read(fp, len);
    for(int i=0; i < len; i++) {
        save->tile[0][i].owner = tiles[i];
    }
    
    free(tiles);
    return len;
}

int ottd_read_DATE(FILE *fp, int verbose, ottd_t *save)
{
    uint32_t len = ottd_read_riff_length(fp);
    if (len == 0) return -1;
    long end = ftell(fp) + len;
    
    // read current date
    int32_t date;
    if (save->version < 31) {
        date = ottd_read_u16(fp) + DAYS_TILL_ORIGINAL_BASE_YEAR;
    } else {
        date = ottd_read_u32(fp);
    }
    
    // deconstruct it
    ConvertDateToYMD(date, &save->curDate);
    
    // skip to end
    fseek(fp, end, SEEK_SET);
    return len;
}

int ottd_read_PATS(FILE *fp, int verbose, ottd_t *save)
{
    uint32_t len = ottd_read_riff_length(fp);
    long end = ftell(fp) + len;
    int version = save->version;
    
    // skip generated with gen_PATS_skip.rb
    ottd_skip(fp, 28);
    if (version >= 148) ottd_skip(fp, 1);
    if (version >= 159) ottd_skip(fp, 5);
    if (version >= 75) ottd_skip(fp, 1);
    if (version >= 97) ottd_skip(fp, 22);
    if (version >= 133) ottd_skip(fp, 1);
    if (version <= 143) ottd_skip(fp, 4);
    if (version >= 38) ottd_skip(fp, 1);
    if (version >= 28 && version <= 86) ottd_skip(fp, 3);
    if (version >= 96) ottd_skip(fp, 1);
    if (version >= 97 && version <= 163) ottd_skip(fp, 1);
    if (version >= 165) ottd_skip(fp, 1);
    if (version >= 114) ottd_skip(fp, 1);
    if (version >= 143) ottd_skip(fp, 1);
    if (version >= 154) ottd_skip(fp, 1);
    if (version >= 139) ottd_skip(fp, 2);
    if (version >= 128) ottd_skip(fp, 1);
    if (version >= 59) ottd_skip(fp, 1);
    if (version >= 97 && version <= 109) ottd_skip(fp, 2);
    if (version >= 106) ottd_skip(fp, 1);
    if (version >= 95) ottd_skip(fp, 1);
    if (version >= 67 && version <= 158) ottd_skip(fp, 1);
    if (version <= 140) ottd_skip(fp, 1);
    if (version >= 62) ottd_skip(fp, 1);
    if (version >= 40) ottd_skip(fp, 1);
    if (version >= 113) ottd_skip(fp, 1);
    if (version >= 175) ottd_skip(fp, 6);
    if (version >= 160) ottd_skip(fp, 1);
    if (version >= 47) ottd_skip(fp, 1);
    if (version >= 138) ottd_skip(fp, 1);
    if (version >= 87) ottd_skip(fp, 3);
    if (version <= 119) ottd_skip(fp, 9);
    if (version <= 86) ottd_skip(fp, 1);
    if (version >= 145) ottd_skip(fp, 1);
    if (version >= 156) ottd_skip(fp, 12);
    if (version >= 97 && version <= 177) ottd_skip(fp, 1);
    if (version >= 79) ottd_skip(fp, 2);
    if (version >= 22 && version <= 92) ottd_skip(fp, 2);
    if (version >= 90) ottd_skip(fp, 1);
    if (version >= 39) ottd_skip(fp, 1);
    if (version <= 158) ottd_skip(fp, 5);
    
    // read start year
    save->startYear = ottd_read_u32(fp);
    
    fseek(fp, end, SEEK_SET);
    return len;
}

void ottd_read_PLYR_economy(FILE *fp, int version, CompanyEconomyEntry *econ)
{
    econ->income = (int64_t)(version < 2)?ottd_read_u32(fp):ottd_read_u64(fp);
    econ->expenses = (int64_t)(version < 2)?ottd_read_u32(fp):ottd_read_u64(fp);
    econ->company_value = (int64_t)(version < 2)?ottd_read_u32(fp):ottd_read_u64(fp);
    econ->delivered_cargo = (int32_t)ottd_read_u32(fp);
    econ->performance_history = (int32_t)ottd_read_u32(fp);
}

int ottd_read_PLYR(FILE *fp, int verbose, ottd_t *save)
{
    uint8_t mark = ottd_read_u8(fp);
    if (mark != 1 && mark != 2) return -1; // array or sparse array marker
    
    // elements
    ottd_company_t *company = &save->company[0];
    uint32_t len;
    while((len = ottd_read_sg(fp))) {
        if (len == 1) {
            company->active = false;
            // skip to next
            company++;
            continue;
        }
        company->active = true;
        long end = ftell(fp) + len - 1;
        // skip name args, openttd strings make me cry
        ottd_skip(fp, 6);
        // read name
        char *name = (save->version >= 84)? ottd_read_str(fp) : NULL;
        if (name == NULL) {
            name = malloc(16);
            sprintf(name, "Company %d", (int)((company-save->company)+1));
        }
        company->name = name;
        
        // skip manager args
        ottd_skip(fp, 6);
        // read manager name
        char *mgr = (save->version >= 84)? ottd_read_str(fp) : NULL;
        company->manager = mgr?mgr:strdup("Unknown");
        
        // read more things
        company->face = ottd_read_u32(fp);
        company->money = (int64_t)(save->version > 0)?ottd_read_u64(fp):ottd_read_u32(fp);
        company->loan = (save->version > 64)?ottd_read_u64(fp):ottd_read_u32(fp);
        company->color = ottd_read_u8(fp);
        
        // skip
        ottd_skip(fp, 1); // money fraction
        if (save->version <= 57) ottd_skip(fp, 1); // available railtypes
        ottd_skip(fp, 1); // block preview
        
        // skip cargo types
        (save->version > 93)?ottd_read_u32(fp):ottd_read_u16(fp);
        // skip hq location
        (save->version > 5)?ottd_read_u32(fp):ottd_read_u16(fp);
        // skip last build coord
        (save->version > 5)?ottd_read_u32(fp):ottd_read_u16(fp);
        // inaugurated year
        if (save->version < 31) {
            company->inaugurated_year = ottd_read_u8(fp) + ORIGINAL_BASE_YEAR;
        } else {
            company->inaugurated_year = (int32_t)ottd_read_u32(fp);
        }
        // skip share owners
        ottd_skip(fp, 4);
        // valid economy entries
        company->num_valid_stat_ent = ottd_read_u8(fp);
        if (company->num_valid_stat_ent > MAX_HISTORY_MONTHS) company->num_valid_stat_ent = MAX_HISTORY_MONTHS;
        // skip bankrupcy data
        ottd_skip(fp, 1 + (save->version>103?2:1) + 2 + (save->version>64?8:4));
        // yearly expenses
        for(int i=0; i < 3; i++) for(int j=0; j < 13; j++) {
            company->yearly_expenses[i][j] = (int64_t)(save->version > 1)?ottd_read_u64(fp):ottd_read_u32(fp);
        }
        // ai
        if (save->version >= 2) company->ai = ottd_read_u8(fp)?true:false;
        if (save->version >= 107 && save->version <= 111) ottd_skip(fp, 1);
        if (save->version >= 4 && save->version <= 99) ottd_skip(fp, 1);
        // skip limits
        if (save->version >= 156) ottd_skip(fp, 8);
        
        // company settings we don't care about
        if (save->version >= 16 && save->version <= 18) ottd_skip(fp, 512);
        if (save->version >= 19 && save->version <= 68) ottd_skip(fp, 2);
        if (save->version >= 69) ottd_skip(fp, 4);
        if (save->version >= 16) ottd_skip(fp, 7);
        if (save->version >= 2) ottd_skip(fp, 1);
        if (save->version >= 120) ottd_skip(fp, 9);
        if (save->version >= 2 && save->version <= 143) ottd_skip(fp, 63);
        
        // old ai settings
        if (company->ai && save->version < 107) {
            ottd_skip(fp, 10);
            ottd_skip(fp, (save->version < 13)?2:4);
            uint8_t num_build_rec = ottd_read_u8(fp);
            ottd_skip(fp, (save->version < 6)?8:16);
            ottd_skip(fp, (save->version < 69)?2:4);
            ottd_skip(fp, 77);
            if (save->version >= 2) ottd_skip(fp, 64);
            
            for(int i=0; i < num_build_rec; i++) {
                ottd_skip(fp, (save->version < 6)?4:8);
                ottd_skip(fp, 8);
            }
        }
        
        // economy
        ottd_read_PLYR_economy(fp, save->version, &company->cur_economy);
        for(int i=0; i < company->num_valid_stat_ent; i++)
            ottd_read_PLYR_economy(fp, save->version, &company->old_economy[i]);
        
        // skip to next
        fseek(fp, end, SEEK_SET);
        company++;
    }
    
    return 0;
}