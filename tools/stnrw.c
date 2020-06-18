//
// vim: sw=4 ts=4 sts=4
//
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#define streq(s1,s2) (strcmp( s1, s2 ) == 0)

#define MAX_EXT_BOARDS    	24 // allow more zones for linux-based firmwares

#define MAX_NUM_BOARDS    (1+MAX_EXT_BOARDS)  // maximum number of 8-zone boards including expanders
#define MAX_NUM_STATIONS  (MAX_NUM_BOARDS*8)  // maximum number of stations

#define TMP_BUFFER_SIZE      255   // scratch buffer size
#define STATION_NAME_SIZE 32    // maximum number of characters in each station name

#define STATION_SPECIAL_DATA_SIZE  (TMP_BUFFER_SIZE - STATION_NAME_SIZE - 12)

/** Station macro defines */
#define STN_TYPE_STANDARD    0x00
#define STN_TYPE_RF          0x01    // Radio Frequency (RF) station
#define STN_TYPE_REMOTE      0x02    // Remote OpenSprinkler station
#define STN_TYPE_GPIO        0x03    // direct GPIO station
#define STN_TYPE_HTTP        0x04    // HTTP station
#define STN_TYPE_OTHER       0xFF

typedef unsigned char byte;

typedef struct StationAttrib {    // station attributes
    byte mas:1;
    byte igs:1;	// ignore sensor 1
    byte mas2:1;
    byte dis:1;
    byte seq:1;
    byte igs2:1;// ignore sensor 2
    byte igrd:1;// ignore rain delay
    byte unused:1;
    
    byte gid:4; // group id: reserved for the future
    byte dummy:4;
    byte reserved[2]; // reserved bytes for the future
} StationAttrib; // total is 4 bytes so far

/** Station data structure */
typedef struct StationData {
    char name[STATION_NAME_SIZE];
    StationAttrib attrib;
    byte type; // station type
    byte sped[STATION_SPECIAL_DATA_SIZE]; // special station data
} StationData;

/** RF station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct RFStationData {
    byte on[6];
    byte off[6];
    byte timing[4];
};

/** Remote station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct RemoteStationData {
    byte ip[8];
    byte port[4];
    byte sid[2];
};

/** GPIO station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
typedef struct GPIOStationData {
    byte pin[3];
    byte active;
} GPIOStationData;

/** HTTP station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct HTTPStationData {
    byte data[STATION_SPECIAL_DATA_SIZE];
};

int read_stationdata( int fd, int index, StationData *sd )
{
    (void) lseek( fd, (off_t) sizeof(StationData) * index, SEEK_SET );
    (void) read( fd, (void *) sd, sizeof(StationData) );

    return 0;
}

int write_stationdata( int fd, int index, StationData *sd )
{
    (void) lseek( fd, (off_t) sizeof(StationData) * index, SEEK_SET );
    (void) write( fd, (void *) sd, sizeof(StationData) );

    return 0;
}

char * station_types[] = {
    "Std", "RF", "Remote", "GPIO", "HTTP"
};
#define NUM_TYPES (sizeof(station_types)/sizeof(char *))

int parse_fields( char ** argv, StationData *sd )
{
    for( ; *argv ; argv++ ) {
        char *f = *argv;
        char *v = index( f, '=' );
        if( v == NULL ) {
            printf("Ill formed field string '%s'\n", f );
            return 1;
        }
        *v++ = '\0';
        int iv = strtoul( v, NULL, 10 );

        if( streq(f, "master") ) {
            sd->attrib.mas = iv;
        } else if( streq(f, "master2") ) {
            sd->attrib.mas2 = iv;
        } else if( streq(f, "disabled") ) {
            sd->attrib.dis = iv;
        } else if( streq(f, "sequential") ) {
            sd->attrib.seq = iv;
        } else if( streq(f, "ignore_sensor1") ) {
            sd->attrib.igs = iv;
        } else if( streq(f, "ignore_sensor2") ) {
            sd->attrib.igs2 = iv;
        } else if( streq(f, "ignore_rain_data") ) {
            sd->attrib.igrd = iv;
        } else if( streq(f, "gpio_pin") ) {
            GPIOStationData  * gd = (GPIOStationData *) sd->sped;
            if( sd->type != STN_TYPE_GPIO )
                gd->active = '0';   // provide a default
            sd->type = STN_TYPE_GPIO;
            if( iv >= 1000 ) {
                printf("GPIO number %d too large (0 <= pin < 1000)\n", iv);
                return 1;
            }
            for( int i = sizeof(gd->pin) - 1 ; i >= 0 ; i--, iv /= 10 ) {
                gd->pin[i] = (iv % 10) + '0';
            }
        } else if( streq(f, "gpio_active_state") ) {
            GPIOStationData  * gd = (GPIOStationData *) sd->sped;
            if( sd->type != STN_TYPE_GPIO ) {
                for( int i = 0 ; i < sizeof(gd->pin) ; i++ )
                    gd->pin[i] = '0';   // provide a default
            }
            sd->type = STN_TYPE_GPIO;
            gd->active = (iv & 1) + '0';
        } else if( streq(f, "name") ) {
            if( strlen(v) >= sizeof(sd->name) - 1 ) {
                printf("Station name '%s' too long (< %d)\n", sizeof(sd->name) - 1);
                return 1;
            }
            strncpy( sd->name, v, sizeof(sd->name) - 1 );
        } else {
            printf("Unknown field name '%s'\n", f );
            return 1;
        }
    }
    return 0;
}

void print_stationattrib( StationAttrib *sa )
{
    printf( "  master %d, master2 %d, disabled %d, sequential %d,\n"
            "  ignore_sensor1 %d, ignore_sensor2 %d ignore_rain_data %d\n",
            sa->mas,
            sa->mas2,
            sa->dis,
            sa->seq,
            sa->igs,
            sa->igs2,
            sa->igrd );
}

void print_stationdata( int index, StationData *sd )
{
    char * typename;

    if( sd->type >= NUM_TYPES )
        typename = "unknown";
    else
        typename = station_types[sd->type];

    printf( "%d : name '%s' type %s\n", index, sd->name, typename );
    print_stationattrib( & sd->attrib );

    if( sd->type == STN_TYPE_GPIO ) {
        GPIOStationData  * gd = (GPIOStationData *) sd->sped;
        int pin = 0;
        int active = gd->active - '0';

        for( int i = 0 ; i < sizeof(gd->pin) ; i++ ) {
            pin = pin * 10 + (gd->pin[i] - '0');
        }
        printf( "  gpio_pin %d, gpio_active_state %d\n", pin, active );
    }
}

void usage( const char * progname )
{
    printf( "usage: %s [-n -q] <stns-dat file> <station-no> [<field>=<value>]*\n", progname );
    printf( "    -n = dry_run - don't actually write to file\n");
    printf( "    -q = quiet - don't print record contents\n");
    printf( "    <stns-dat file> - file name of a stns.dat formatted file\n");
    printf( "    <station-no>    - station number (1..%d)\n", MAX_NUM_STATIONS );
    printf( "    Field/value pairs provide new values to the selected record\n" );
    printf( "    Valid field names:\n"
            "      name        - string station name ( < %d chars)\n"
            "      master      - station is dependent on master (0|1)\n"
            "      master2     - station is dependent on master2 (0|1)\n"
            "      disabled    - station is disabled (0|1)\n"
            "      sequential  - station scheduled sequentially (0|1)\n"
            "      ignore_sensor1    - ignore water sensor 1\n"
            "      ignore_sensor2    - ignore water sensor 2\n"
            "      ignore_rain_data  - ignore current rain data/history\n"
            "      gpio_pin          - Linux GPIO pin number\n"
            "      gpio_active_state - pin state when activated (0|1)\n",
            STATION_NAME_SIZE - 1);
    exit(1);
}

int main( int argc, char ** argv )
{
    // printf( "StationAttrib = %d\n", sizeof(struct StationAttrib) );
    // printf( "StationData = %d\n", sizeof(struct StationData) );
    // printf( "StationData.type = %d\n", offsetof(struct StationData,type) );
    // printf( "STATION_SPECIAL_DATA_SIZE = %d\n", STATION_SPECIAL_DATA_SIZE  );
    int quiet = 0;
    int dry_run = 0;
    const char * progname = *argv++;
    argc--;

    while( *argv && *argv[0] == '-' ) {
        if( streq( *argv, "-q" ) )
            quiet = 1;
        else if( streq( *argv, "-n" ) )
            dry_run = 1;
        else {
            printf("Unknown option '%s'\n", *argv );
            usage( progname );
        }
        argv++;
        argc--;
    }

    if( argc < 2 )
        usage( progname );

    int fd = open( *argv, O_RDWR );

    if( fd < 0 ) {
        printf("Can't open '%s'\n", *argv );
        exit(1);
    }
    argv++;

    const char * str = *argv;

    while( *str )
        if( ! isdigit(*str++) ) {
            printf("Station number '%s' is not a number!\n", *argv);
            usage( progname );
        }
    int index = strtoul( *argv, NULL, 10 );
    if( index < 1 || index > MAX_NUM_STATIONS ) {
        printf("Station number '%d' out of range.\n", index);
        usage( progname );
    }
    index -= 1; // convert from station number to array index
    
    argv++;

    StationData sd;

    read_stationdata( fd, index, & sd );

    if( argc > 2 ) {
        if( parse_fields( argv, & sd ) )
            usage( progname );
        if( ! dry_run )
            write_stationdata( fd, index, & sd );
    }
    if( ! quiet )
        print_stationdata( index, & sd );

    exit(0);
}
