#define _POSIX_C_SOURCE 200809L

#include "debug.h"

#include "log.h"
#include "config.h" 

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

bool debug_dump_escpos(const buffer_t *escpos)
{
    struct stat st;

    time_t now;
    struct tm tm;

    char filename[512];

    FILE *fp;

    if (stat(DEBUG_DIRECTORY, &st) != 0)
    {
        if (mkdir(DEBUG_DIRECTORY, 0755) != 0)
        {
            log_warning("Unable to create %s: %s", DEBUG_DIRECTORY, strerror(errno));
            return false;
        }
    }
    else if (!S_ISDIR(st.st_mode))
    {
        log_warning("%s exists but is not a directory", DEBUG_DIRECTORY);
        return false;

    }

    now = time(NULL);

    localtime_r(&now, &tm);

    if (strftime(filename, sizeof(filename), DEBUG_DIRECTORY "/job-%Y%m%d-%H%M%S.escpos", &tm) == 0)
    {
        log_warning("Unable to generate debug filename");
        return false;
    }

    fp = fopen(filename, "wb");

    if (fp == NULL)
    {
        log_warning("Unable to create %s: %s", filename, strerror(errno));
        return false;
    }

    if (fwrite(buffer_data(escpos), 1, buffer_length(escpos), fp) != buffer_length(escpos))
    {
        log_warning("Error writing %s: %s", filename, strerror(errno));
        fclose(fp);
        return false;
    }

    fclose(fp);

    log_debug("ESC/POS job written to %s", filename);

    return true;
}
