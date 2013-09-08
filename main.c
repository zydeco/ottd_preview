#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <getopt.h>
#include "ottd.h"

void print_usage(int end)
{
    fprintf(stderr, "Usage: ottd_preview file [-v] [-m nw|ne|iso] [-d output.txt] [-p output.png]\n");
    if (end) exit(1);
}

void print_help(void)
{
    print_usage(0);
    printf("Generate a preview of an OpenTTD save/scenario\n");
    printf("Options:\n");
    printf(" -v|--verbose\n");
    printf(" -p|--png <output>  write map image\n");
    printf(" -m|--map           map orientation (nw,ne,iso)\n");
    printf(" -d|--data <output> write map info (company names and colors, plain text)\n");
    printf(" -h|--help          show this help\n");
    exit(1);
}

int main (int argc, char * const *argv)
{
    char *png_output = NULL;
    char *data_output = NULL;
    char *file_path = NULL;
    int verbose = 0, map_mode = 0;
    
    // parse args
    int opt;
    const struct option opts[] = {
        {"verbose", no_argument, NULL, 'v'},
        {"png", required_argument, NULL, 'p'},
        {"data", required_argument, NULL, 'd'},
        {"help", required_argument, NULL, 'h'},
        {"map", required_argument, NULL, 'm'},
        {0, 0, 0, 0}
    };
    while((opt = getopt_long(argc, argv, "vp:d:m:h?", opts, NULL)) != -1) {
        switch(opt) {
            case 'v':
                verbose = 1;
                break;
            case 'p':
                png_output = strdup(optarg);
                break;
            case 'd':
                data_output = strdup(optarg);
                break;
            case 'm':
                // map mode (nw, ne, iso)
                if (strcasecmp(optarg, "nw") == 0) map_mode = OTTD_MAP_NW;
                else if (strcasecmp(optarg, "ne") == 0) map_mode = OTTD_MAP_NE;
                else if (strcasecmp(optarg, "iso") == 0) map_mode = OTTD_MAP_ISO;
                else print_help();
                break;
            case '?':
            case 'h':
                print_help();
        }
    }
    if (argc - optind != 1) print_usage(1);
    file_path = argv[optind];
    
    // load game
    ottd_t *game = ottd_load(file_path, verbose);
    if (game == NULL) fprintf(stderr, "ottd_preview: %s\n", strerror(errno));
    
    // save data
    if (game && data_output) {
        FILE *fp = fopen(data_output, "w");
        for(int i=0; i < 15; i++) {
            ottd_company_t *cmp= &game->company[i];
            if (!cmp->active) continue;
            png_color cc = ottd_color[ottd_company_color(cmp->color)];
            fprintf(fp, "Company %d #%02X%02X%02X %s\n", i+1, cc.red,cc.green,cc.blue, cmp->name);
        }
        fprintf(fp, "# data end\n");
        fclose(fp);
    }
    
    // save png
    if (game && png_output) {
        char *modestr[] = {"nw", "ne", "iso"};
        if (verbose) printf("writing png to %s (%s)\n", png_output, modestr[map_mode]);
        ottd_write_png(game, png_output, map_mode);
    }
    
    if (verbose) {
        char yearsText[32];
        printf("Savegame Version: %d\n", game->version);
        snprintf(yearsText, sizeof yearsText, "%d-%d", game->startYear, game->curDate.year);
        printf("Years: %s\n", yearsText);
        for(int i=0; i < 15; i++) {
            ottd_company_t *cmp= &game->company[i];
            if (!cmp->active) continue;
            printf("Company %d: %s\n", i+1, cmp->name);
        }
    }
    
    // free the memory
    ottd_free(game);
    free(png_output);
    free(data_output);
    
    return 0;
}
