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
#include <stdbool.h>

#ifdef HAVE_LIBPNG
#include <png.h>
#else
typedef struct png_color {
    uint8_t red, green, blue;
} png_color;
#endif

#define MAX_HISTORY_MONTHS 24
#define DAYS_TILL_ORIGINAL_BASE_YEAR 701265

typedef enum TileType {
    MP_CLEAR,               ///< A tile without any structures, i.e. grass, rocks, farm fields etc.
    MP_RAILWAY,             ///< A railway
    MP_ROAD,                ///< A tile with road (or tram tracks)
    MP_HOUSE,               ///< A house by a town
    MP_TREES,               ///< Tile got trees
    MP_STATION,             ///< A tile of a station
    MP_WATER,               ///< Water tile
    MP_VOID,                ///< Invisible tiles at the SW and SE border
    MP_INDUSTRY,            ///< Part of an industry
    MP_TUNNELBRIDGE,        ///< Tunnel entry/exit and bridge heads
    MP_OBJECT,              ///< Contains objects such as transmitters and owned land
} TileType;

typedef enum ExpensesType {
    EXPENSES_CONSTRUCTION =  0,   ///< Construction costs.
    EXPENSES_NEW_VEHICLES,        ///< New vehicles.
    EXPENSES_TRAIN_RUN,           ///< Running costs trains.
    EXPENSES_ROADVEH_RUN,         ///< Running costs road vehicles.
    EXPENSES_AIRCRAFT_RUN,        ///< Running costs aircrafts.
    EXPENSES_SHIP_RUN,            ///< Running costs ships.
    EXPENSES_PROPERTY,            ///< Property costs.
    EXPENSES_TRAIN_INC,           ///< Income from trains.
    EXPENSES_ROADVEH_INC,         ///< Income from road vehicles.
    EXPENSES_AIRCRAFT_INC,        ///< Income from aircrafts.
    EXPENSES_SHIP_INC,            ///< Income from ships.
    EXPENSES_LOAN_INT,            ///< Interest payments over the loan.
    EXPENSES_OTHER,               ///< Other expenses.
    EXPENSES_END,                 ///< Number of expense types.
    INVALID_EXPENSES      = 0xFF, ///< Invalid expense type.
} ExpensesType;

typedef enum TileOwner {
    // 0-14 companies
    OWNER_TOWN      = 0x0F,  ///< A town owns this tile
    OWNER_NOBODY    = 0x10,
    OWNER_WATER     = 0x11,
    OWNER_SPECTATOR = 0xFF,
} TileOwner;

typedef struct ottd_tile {
    TileType    type;
    uint8_t     height;
    TileOwner   owner;
} ottd_tile_t;

typedef struct CompanyEconomyEntry {
    int64_t income;
    int64_t expenses;
    int32_t delivered_cargo;
    int32_t performance_history; ///< company score (scale 0-1000)
    int64_t company_value;
} CompanyEconomyEntry;

typedef struct ottd_company {
    bool        active;
    bool        ai;
    char        *name;
    char        *manager;
    uint32_t    face;
    int32_t     inaugurated_year;
    int64_t     money, loan;
    uint8_t     color;
    
    // economy
    int64_t yearly_expenses[3][EXPENSES_END];            ///< Expenses of the company for the last three years, in every #Expenses category.
    CompanyEconomyEntry cur_economy;                     ///< Economic data of the company of this quarter.
    CompanyEconomyEntry old_economy[MAX_HISTORY_MONTHS]; ///< Economic data of the company of the last #MAX_HISTORY_MONTHS months.
    uint8_t num_valid_stat_ent;                          ///< Number of valid statistical entries in #old_economy.
} ottd_company_t;

typedef struct YearMonthDay {
    int32_t year;   ///< Year (0...)
    uint8_t month;  ///< Month (0..11)
    uint8_t day;    ///< Day (1..31)
} YearMonthDay;

typedef struct {
    uint16_t version;
    struct {
        uint32_t x,y;
    } mapSize;
    ottd_company_t company[15];
    int32_t startYear;
    YearMonthDay curDate;
    ottd_tile_t **tile;
} ottd_t;

// map mode for writing png
enum MapMode {
    OTTD_MAP_NW,    // flat view, top is north-west
    OTTD_MAP_NE,    // flat view, top is north-east
    OTTD_MAP_ISO    // isometric view, like openttd smallmap
};

ottd_t* ottd_load(const char *path, int verbose);
void ottd_free(ottd_t* ottd);

// colors everywhere

enum SmallMapColour {
    SM_COLOUR_BLACK     = 0,
    SM_COLOUR_NOT_OWNED = 98,
    SM_COLOUR_TOWN      = 180,
    SM_COLOUR_ROAD      = 136,
    SM_COLOUR_WATER     = 202,
    SM_COLOUR_INDUSTRY  = 32,
    SM_COLOUR_OBJECT    = 92
};

extern png_color ottd_color[256];
uint8_t ottd_tile_color(const ottd_t *game, const ottd_tile_t *tile);
int ottd_company_color(int c);
#ifdef HAVE_LIBPNG
int ottd_write_png(const ottd_t *game, const char *png_path, int mode);
#endif

// date functions
void ConvertDateToYMD(int32_t date, YearMonthDay *ymd);
