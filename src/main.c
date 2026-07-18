/******************************************************************************
 * tmbridge
 *
 * Epson ePOS Bridge
 *
 * Copyright (C) Valper Soluciones y Mantenimientos S.L.
 *
 * main.c
 ******************************************************************************/

#include <stdlib.h>

#include "config.h"
#include "listener.h"
#include "log.h"
#include "epos.h"
#include "http.h"

#define TMBRIDGE_NAME    "tmbridge"
#define TMBRIDGE_VERSION "0.1.0"

int main(int argc, char *argv[])
{
    const char *config = "/etc/tmbridge/tmbridge.conf";

    if (argc > 1)
    {
        config = argv[1];
    }

    if (!config_load(config)) 
    {
        return EXIT_FAILURE;
    }

    log_info("%s %s", TMBRIDGE_NAME, TMBRIDGE_VERSION);

    if (!http_init()) {
        return EXIT_FAILURE;
    }
    
    if (!listener_start())
    {
        http_cleanup();
        return EXIT_FAILURE;
    }

    http_cleanup();

    return EXIT_SUCCESS;
}
