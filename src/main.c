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

int main(void)
{
    log_info("%s %s", TMBRIDGE_NAME, TMBRIDGE_VERSION);

    if (!config_load("tmbridge.conf"))
        return EXIT_FAILURE;

    if (!http_init())
        return EXIT_FAILURE;

    if (!listener_start())
    {
        http_cleanup();
        return EXIT_FAILURE;
    }

    http_cleanup();

    return EXIT_SUCCESS;
}
