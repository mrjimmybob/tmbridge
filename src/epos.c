/******************************************************************************
 * tmbridge

 *
 * epos.c
 ******************************************************************************/

#include "epos.h"

#include "buffer.h"
#include "config.h"
#include "http.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

/*****************************************************************************/

static bool xml_escape(buffer_t *xml,
                       const char *text)
{
    while (*text != '\0')
    {
        switch (*text)
        {
            case '&':
                if (!buffer_append_string(xml, "&amp;"))
                    return false;
                break;

            case '<':
                if (!buffer_append_string(xml, "&lt;"))
                    return false;
                break;

            case '>':
                if (!buffer_append_string(xml, "&gt;"))
                    return false;
                break;

            case '\'':
                if (!buffer_append_string(xml, "&apos;"))
                    return false;
                break;

            case '"':
                if (!buffer_append_string(xml, "&quot;"))
                    return false;
                break;

            default:
                if (!buffer_append_char(xml, *text))
                    return false;
                break;
        }

        text++;
    }

    return true;
}

/*****************************************************************************/

bool epos_print(const char *text)
{
    buffer_t xml;
    buffer_t response;
    char url[512];
    bool ok;

    if (!buffer_init(&xml))
    {
        log_error("Unable to allocate XML buffer");
        return false;
    }

    if (!buffer_init(&response))
    {
        buffer_free(&xml);
        log_error("Unable to allocate response buffer");
        return false;
    }

    snprintf(url,
             sizeof(url),
             "https://%s:%u/cgi-bin/epos/service.cgi?"
             "devid=%s&timeout=%d",
             config_get_printer_host(),
             config_get_printer_port(),
             config_get_device(),
             config_get_timeout() * 1000);

    log_debug("URL: %s", url);

    if (!buffer_append_string(&xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>")) {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }
    if (!buffer_append_string(&xml, "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">")) {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }
    if (!buffer_append_string(&xml, "<s:Body>")) {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }

    if (!buffer_append_string(&xml, "<epos-print xmlns=\"http://www.epson-pos.com/schemas/2011/03/epos-print\">"))
    {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }

    if (!buffer_append_string(&xml, "<text lang=\"en\" smooth=\"true\">"))
    {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }

    if (!xml_escape(&xml, text))
    {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }

    if (!buffer_append_string(&xml, "</text>"))
    {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }

    if (!buffer_append_string(&xml, "<cut type=\"feed\"/>"))
    {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }

    if (!buffer_append_string(&xml, "</epos-print>"))
    {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }
    if (!buffer_append_string(&xml, "</s:Body>"))
    {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }
    if (!buffer_append_string(&xml, "</s:Envelope>"))
    {
	buffer_free(&response);
	buffer_free(&xml);
	return false;
    }

    log_debug("XML:");
    log_debug("%s", buffer_data(&xml));

    ok = http_post(
            url,
            "text/xml; charset=utf-8",
            buffer_data(&xml),
            buffer_length(&xml),
            &response);
    if (!ok)
    {
        log_error("Printer request failed");
	buffer_free(&response);
	buffer_free(&xml);
    	return false;
    }
    if (buffer_length(&response) > 0)
    {
        log_debug("Printer response:");
        log_debug("%s", buffer_data(&response));
    }

    buffer_free(&response);
    buffer_free(&xml);

    return true;
}

