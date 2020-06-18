#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#define streq(s1,s2) (strcmp( s1, s2 ) == 0)
#define streqn(s1,s2,l) (strncmp( s1, s2, l ) == 0)

#define MAX_SOPTS_SIZE  160

typedef struct sopt {
    const char *name;
    uint32_t    offset;
} Sopt;

Sopt Sopts[] = {
    { .name = "password" ,       .offset = 0 },
    { .name = "location" ,       .offset = MAX_SOPTS_SIZE * 1 },
    { .name = "javascript_URL" , .offset = MAX_SOPTS_SIZE * 2 },
    { .name = "weather_URL" ,    .offset = MAX_SOPTS_SIZE * 3 },
    { .name = NULL }    // terminator
};

typedef struct soptData {
    char data[MAX_SOPTS_SIZE];
} SoptData;

Sopt * parse_fields( char * arg, SoptData * sd )
{
    memset( sd->data, 0, sizeof(sd->data) );

    char *f = arg;
    char *v = index( f, '=' );
    if( v == NULL ) {
        printf("Ill formed field string '%s'\n", f );
        return NULL;
    }
    *v++ = '\0';

    for( Sopt *sopt = Sopts ; sopt->name ; sopt++ ) {

        if( streq(f, sopt->name) ) {
            strncpy( sd->data, v, strlen(v) );
            return sopt;
        }
    }
    return NULL;
}

int print_sopt( int fd, Sopt * sopt, SoptData * sd )
{
    (void) lseek( fd, (off_t) sopt->offset, SEEK_SET );
    (void) read( fd, (void *) sd->data, sizeof(sd->data) );

    printf("Sopt '%s' = '%s'\n", sopt->name, sd->data );

    return 0;
}

int write_sopt( int fd, Sopt * sopt, SoptData * sd )
{
    (void) lseek( fd, (off_t) sopt->offset, SEEK_SET );
    (void) write( fd, (void *) sd->data, sizeof(sd->data) );

    return 0;
}


void usage( const char * progname )
{
    printf( "usage: %s [-n -q] <sopts-dat file> [<opt>=<value>]*\n", progname );
    printf( "    -n = dry_run - don't actually write to file\n");
    printf( "    -q = quiet - don't print record contents\n");
    printf( "    <sopts-dat file> - file name of a sopts.dat formatted file\n");
    printf( "    Opt/value pairs provide new values to the selected record\n" );
    printf( "    A Opt= (no value) will cause the current file value to be printed\n" );
    printf( "    Valid opt names:\n"
            "      password        - string password (hashed ?)\n"
            "      location        - string GPS coords\n"
            "      javascript_URL  - string URL for webgui 'js' dir\n"
            "      weather_URL     - string URL for opensprinkler weather service\n"
            );
    exit(1);
}

int main( int argc, char ** argv )
{
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

    while( *argv ) {
        SoptData  sd;
        Sopt *    sopt = parse_fields( *argv++, & sd );

        if( sopt == NULL )
            usage( progname );
        if( ! dry_run )
            if( sd.data[0] == '\0' )
                print_sopt( fd, sopt, & sd );
            else
                write_sopt( fd, sopt, & sd );
    }

    exit(0);
}
