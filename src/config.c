/******************************************************************************
 * tmbridge

 *
 * config.c
 ******************************************************************************/

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"


typedef struct
{
    char listen_host[64];
    uint16_t listen_port;
    char printer_host[64];
    uint16_t printer_port;
    char device[64];
    bool verify_tls;
    int http_timeout;
    int epos_timeout;
    bool debug_xml;
    bool debug_escpos;

} config_t;

static config_t g_config;

/*****************************************************************************/

static char *trim(char *s)
{
    char *end;

    while (isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
        return s;

    end = s + strlen(s) - 1;

    while (end > s && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';

    return s;

}


/*****************************************************************************/

static void config_defaults(void)
{
    snprintf(g_config.listen_host, sizeof(g_config.listen_host), "%s", "127.0.0.1");
    g_config.listen_port = 9100;
    g_config.printer_host[0] = '\0';
    g_config.printer_port = 443;
    snprintf(g_config.device, sizeof(g_config.device), "%s", "local_printer");
    g_config.verify_tls = false;
    g_config.http_timeout = 20;
    g_config.epos_timeout = 10000;
    g_config.debug_xml = false;
    g_config.debug_escpos = false;
}

/*****************************************************************************/

static bool config_validate(void)
{
    if (g_config.printer_host[0] == '\0')
    {
        log_error("Configuration error: printer_host is missing.");
        return false;
    }

    if (g_config.listen_port == 0)
    {
        log_error("Configuration error: invalid listen_port.");
        return false;
    }

    if (g_config.printer_port == 0)
    {
        log_error("Configuration error: invalid printer_port.");
        return false;
    }

    if (g_config.http_timeout <= 0)
    {
        log_error("Configuration error: invalid http timeout.");
        return false;
    }
    
    if (g_config.epos_timeout <= 0)
    {
        log_error("Configuration error: invalid epos timeout.");
        return false;
    }


    return true;
}

/*****************************************************************************/


bool config_load(const char *filename)
{
    FILE *fp;
    char line[256];

    config_defaults();

    fp = fopen(filename, "r");

    if (fp == NULL)
    {
        log_error("Cannot open configuration file '%s'.", filename);
        return false;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *key;
        char *value;
        char *eq;

        key = trim(line);

        if (*key == '\0' || *key == '#')
            continue;

        eq = strchr(key, '=');


        if (eq == NULL)
            continue;

        *eq = '\0';

        value = trim(eq + 1);

        key   = trim(key);

        if (strcmp(key, "listen_host") == 0)
        {
            snprintf(g_config.listen_host, sizeof(g_config.listen_host), "%s", value);
        }
        else if (strcmp(key, "listen_port") == 0)
        {
            g_config.listen_port = (uint16_t)strtoul(value, NULL, 10);
        }
        else if (strcmp(key, "printer_host") == 0)
        {
            snprintf(g_config.printer_host, sizeof(g_config.printer_host), "%s", value);
        }
        else if (strcmp(key, "printer_port") == 0)
        {
            g_config.printer_port = (uint16_t)strtoul(value, NULL, 10);
        }
        else if (strcmp(key, "device") == 0)
        {
            snprintf(g_config.device, sizeof(g_config.device), "%s", value);
        }
        else if (strcmp(key, "verify_tls") == 0)
        {
            g_config.verify_tls = (strtoul(value, NULL, 10) != 0);
        }
        else if (strcmp(key, "http_timeout") == 0)
        {
            g_config.http_timeout = (int)strtol(value, NULL, 10);
        }
        else if (strcmp(key, "epos_timeout") == 0)
        {
            g_config.epos_timeout = (int)strtol(value, NULL, 10);
        }
	else if (strcmp(key, "debug_xml") == 0)
	{
	    g_config.debug_xml = atoi(value) != 0;
	}
	else if (strcmp(key, "debug_escpos") == 0)
	{
	    g_config.debug_escpos = atoi(value) != 0;
	}
        else
        {
            log_error("Unknown configuration key '%s'.", key);
        }
    }

    fclose(fp);

    return config_validate();
}

/*****************************************************************************/

const char *config_get_listen_host(void)
{
    return g_config.listen_host;
}

/*****************************************************************************/

uint16_t config_get_listen_port(void)
{
    return g_config.listen_port;
}

/*****************************************************************************/

const char *config_get_printer_host(void)
{
    return g_config.printer_host;
}

/*****************************************************************************/

uint16_t config_get_printer_port(void)
{
    return g_config.printer_port;
}

/*****************************************************************************/

const char *config_get_device(void)
{
    return g_config.device;
}

/*****************************************************************************/

bool config_get_verify_tls(void)
{
    return g_config.verify_tls;
}

/*****************************************************************************/

int config_get_http_timeout(void)
{
    return g_config.http_timeout;
}

/*****************************************************************************/

int config_get_epos_timeout(void)
{
    return g_config.epos_timeout;
}

/*****************************************************************************/

bool config_get_debug_xml(void)
{
    return g_config.debug_xml;
}

/*****************************************************************************/

bool config_get_debug_escpos(void)
{
    return g_config.debug_escpos;
}

