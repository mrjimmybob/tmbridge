#define _POSIX_C_SOURCE 200809L

#include "escpos.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define XML_NS "http://www.epson-pos.com/schemas/2011/03/epos-print"

typedef struct
{
    buffer_t *xml;
    buffer_t text;

    const char *align;

    bool em;
    bool dw;
    bool dh;
    bool ul;
    bool reverse;

    uint8_t *qr_data;
    size_t qr_length;

    unsigned qr_size;
    const char *qr_level;

} escpos_state_t;

static bool xml_escape_char(unsigned char c, buffer_t *out);

static size_t parse_barcode(escpos_state_t *state, const uint8_t *data, size_t length, size_t index);

static size_t parse_qr(escpos_state_t *state, const uint8_t *data, size_t length, size_t index);

static size_t parse_raster(escpos_state_t *state, const uint8_t *data, size_t length, size_t index);


static const char *bool_string(bool value)
{
    return value ? "true" : "false";
}


static void reset_style(escpos_state_t *state)
{
    state->align   = "left";

    state->em      = false;
    state->dw      = false;
    state->dh      = false;
    state->ul      = false;
    state->reverse = false;

    state->qr_size  = 3;
    state->qr_level = "default";
}


static bool qr_store_data(escpos_state_t *state, const uint8_t *data, size_t length)
{
    uint8_t *copy;

    copy = malloc(length);

    if (copy == NULL)
        return false;

    memcpy(copy, data, length);

    free(state->qr_data);

    state->qr_data = copy;

    state->qr_length = length;

    return true;
}


static bool flush_text(escpos_state_t *state, bool newline)
{
    if (buffer_length(&state->text) == 0 && !newline)
        return true;

    if (!buffer_append_format(
            state->xml,
            "<text "
            "align=\"%s\" "
            "em=\"%s\" "
            "dw=\"%s\" "
            "dh=\"%s\" "
            "ul=\"%s\" "
            "reverse=\"%s\">",
            state->align,
            bool_string(state->em),
            bool_string(state->dw),
            bool_string(state->dh),
            bool_string(state->ul),
            bool_string(state->reverse)))
    {

        return false;
    }

    /* state->text already holds XML-escaped bytes (via xml_escape_char in
       append_text_byte); copy verbatim, do not escape a second time. */
    if (!buffer_append(state->xml,
                       buffer_data(&state->text),
                       buffer_length(&state->text)))
        return false;

    if (newline)
    {
        if (!buffer_append_string(state->xml, "&#10;"))
            return false;
    }

    if (!buffer_append_string(state->xml, "</text>"))
        return false;

    buffer_clear(&state->text);

    return true;
}


static bool emit(escpos_state_t *state, const char *fragment)
{
    if (!flush_text(state, false))
        return false;

    return buffer_append_string(state->xml, fragment);
}


static const char *qr_level(uint8_t level)
{
    switch (level)
    {
        case 48:
            return "level_l";

        case 49:
            return "level_m";

        case 50:
            return "level_q";

        case 51:
            return "level_h";

        default:
            return "default";
    }
}


static bool need(size_t index, size_t length, size_t count)
{
    return (index + count) <= length;
}


static bool append_text_byte(escpos_state_t *state, uint8_t value)
{
    return xml_escape_char(value, &state->text);
}

static size_t parse_esc(escpos_state_t *state, const uint8_t *data, size_t length, size_t index)
{
    uint8_t c;

    if (!need(index, length, 2))
        return length;

    c = data[index + 1];

    switch (c)
    {
        case 0x40:      /* ESC @ */
            flush_text(state, false);
            reset_style(state);
            return index + 2;

        case 0x61:      /* ESC a n */
            if (!need(index, length, 3))
                return length;

            flush_text(state, false);

            switch (data[index + 2])
            {
                case 0:
                case 48:
                    state->align = "left";
                    break;

                case 1:
                case 49:
                    state->align = "center";
                    break;

                case 2:
                case 50:
                    state->align = "right";
                    break;

                default:
                    state->align = "left";
                    break;
            }

            return index + 3;

        case 0x45:      /* ESC E n */
            if (!need(index, length, 3))
                return length;

            flush_text(state, false);

            state->em = (data[index + 2] & 1) != 0;

            return index + 3;

        case 0x2D:      /* ESC - n */
            if (!need(index, length, 3))
                return length;

            flush_text(state, false);

            state->ul = data[index + 2] != 0;

            return index + 3;

        case 0x21:      /* ESC ! n */
        {
            uint8_t mode;

            if (!need(index, length, 3))
                return length;

            mode = data[index + 2];

            flush_text(state, false);

            state->em = (mode & 0x08) != 0;
            state->dh = (mode & 0x10) != 0;
            state->dw = (mode & 0x20) != 0;
            state->ul = (mode & 0x80) != 0;

            return index + 3;
        }

        case 0x64:      /* ESC d n */
            if (!need(index, length, 3))
                return length;
	    char xmld[64];
	    snprintf(xmld, sizeof(xmld), "<feed line=\"%u\"/>", data[index + 2]);
	    emit(state, xmld);
            /* emit(state, "<feed line=\"1\"/>"); */

            return index + 3;

        case 0x4A:      /* ESC J n */
        {
            if (!need(index, length, 3))
                return length;
	    char xmlj[64];
            snprintf(xmlj, sizeof(xmlj), "<feed unit=\"%u\"/>", data[index + 2]);
            emit(state, xmlj);

            return index + 3;
        }

        case 0x70:      /* ESC p */
            emit(state, "<pulse drawer=\"1\" time=\"pulse_100\"/>");

            if (need(index, length, 5))
                return index + 5;

            return length;

        default:
            return index + 2;
    }
}


static size_t parse_gs(escpos_state_t *state, const uint8_t *data, size_t length, size_t index)
{
    uint8_t c;

    if (!need(index, length, 2))
        return length;

    c = data[index + 1];

    switch (c)
    {
        case 0x21:      /* GS ! */
        {
            uint8_t mode;

            if (!need(index, length, 3))
                return length;

            mode = data[index + 2];

            flush_text(state, false);

            state->dw = ((mode >> 4) & 0x0F) != 0;
            state->dh = (mode & 0x0F) != 0;

            return index + 3;
        }

        case 0x42:      /* GS B */
            if (!need(index, length, 3))
                return length;

            flush_text(state, false);


            state->reverse = data[index + 2] != 0;

            return index + 3;

        case 0x56:      /* GS V */
            emit(state, "<cut type=\"feed\"/>");

            if (need(index, length, 3))
            {
                if (data[index + 2] == 65 ||
                    data[index + 2] == 66)
                {
                    if (need(index, length, 4))
                        return index + 4;

                    return length;
                }

                return index + 3;
            }

            return length;

        case 0x6B:
            return parse_barcode(
                state,
                data,
                length,
                index);

        case 0x28:
            if (need(index, length, 3) &&
                data[index + 2] == 0x6B)
            {
                return parse_qr(
                    state,
                    data,
                    length,
                    index);
            }

            return index + 2;

        case 0x76:
            if (need(index, length, 3) &&
                data[index + 2] == 0x30)
            {
                return parse_raster(
                    state,
                    data,
                    length,
                    index);
            }

            return index + 2;

        default:
            return index + 2;
    }
}

static const char *barcode_type(uint8_t type)
{
    switch (type)
    {
        case 0:
        case 65:
            return "upc_a";

        case 1:
        case 66:
            return "upc_e";

        case 2:
        case 67:
            return "ean13";

        case 3:
        case 68:
            return "ean8";

        case 4:
        case 69:
            return "code39";

        case 5:
        case 70:
            return "itf";

        case 6:
        case 71:
            return "codabar";

        case 7:
        case 72:
            return "code93";

        default:
            return "code128";
    }
}

/* CP437 (the ESC/POS default code page) high half, 0x80-0xFF, mapped to
   Unicode code points. ESC/POS text bytes >= 0x80 are CP437, NOT Latin-1, so
   they must be translated here before being emitted as XML numeric refs. */
static const uint16_t cp437_high[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, /* 80 */
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, /* 88 */
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, /* 90 */
    0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192, /* 98 */
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, /* A0 */
    0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB, /* A8 */
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, /* B0 */
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510, /* B8 */
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, /* C0 */
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567, /* C8 */
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, /* D0 */
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580, /* D8 */
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, /* E0 */
    0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229, /* E8 */
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, /* F0 */
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0  /* F8 */
};

static bool xml_escape_char(unsigned char c, buffer_t *out)
{
    switch (c)
    {
        case '&':
            return buffer_append_string(out, "&amp;");
        case '<':
            return buffer_append_string(out, "&lt;");
        case '>':
            return buffer_append_string(out, "&gt;");
        case '\'':
            return buffer_append_string(out, "&apos;");
        case '"':
            return buffer_append_string(out, "&quot;");
        case '\t':
            return buffer_append_char(out, '\t');
        default:
            break;
    }

    /* Drop C0 control bytes (except the tab handled above): they are illegal
       in XML 1.0 and would make the printer reject the whole job. */
    if (c < 0x20)
        return true;

    if (c < 0x80)
        return buffer_append_char(out, (char)c);

    /* CP437 high half -> Unicode numeric character reference. */
    return buffer_append_format(out, "&#%u;", (unsigned)cp437_high[c - 0x80]);
}

static size_t parse_barcode(escpos_state_t *state, const uint8_t *data, size_t length, size_t index)
{
    size_t i;
    size_t start;
    size_t barcode_length;
    const char *type;
    uint8_t m;

    /* index points at GS; layout is GS(0x1D) k(0x6B) m ...
       so the barcode selector m is at index+2, not index+1. */
    if (!need(index, length, 3))
        return length;

    m = data[index + 2];
    type = barcode_type(m);

    /* Newer ESC/POS: GS k m n d1...dn */
    if (m >= 65)
    {
        if (!need(index, length, 4))
            return length;

        barcode_length = data[index + 3];
        start = index + 4;

        if (!need(start, length, barcode_length))
            return length;
    }
    else
    {
        /* Older ESC/POS: GS k m d1...dn NUL */

        start = index + 3;
        i = start;

        while (i < length && data[i] != 0)
            i++;

        barcode_length = i - start;

        if (i == length)
            return length;
    }

    if (!flush_text(state, false))
        return length;

    if (!buffer_append_format(state->xml, "<barcode type=\"%s\">", type))
    {
        return length;
    }

    for (i = 0; i < barcode_length; i++)
    {
        if (!xml_escape_char(data[start + i], state->xml)) 
	{
            return length;
	}
    }

    if (!buffer_append_string(state->xml, "</barcode>"))
        return length;

    if (m >= 65)
        return start + barcode_length;

    return start + barcode_length + 1;
}


static size_t parse_qr(escpos_state_t *state, const uint8_t *data, size_t length, size_t index)
{
    size_t payload_length;
    size_t end;

    uint8_t cn;
    uint8_t fn;

    if (!need(index, length, 7))
        return length;

    payload_length = (size_t)data[index + 3] + ((size_t)data[index + 4] << 8);

    end = index + 5 + payload_length;

    if (end > length)
        return length;

    cn = data[index + 5];
    fn = data[index + 6];

    if (cn == 49 && fn == 67)
    {
        if (need(index, length, 8))
            state->qr_size = (unsigned)data[index + 7];

        if (state->qr_size < 1)

            state->qr_size = 1;

        if (state->qr_size > 8)
            state->qr_size = 8;
    }
    else if (cn == 49 && fn == 69)
    {
        if (need(index, length, 8))
            state->qr_level = qr_level(data[index + 7]);
    }
    else if (cn == 49 && fn == 80)
    {
        /* Store QR data */
        if (end < index + 8)
            return end;

    	if (!qr_store_data(state, data + index + 8, end - (index + 8)))
	{
            return length;
	}
    }   
    else if (cn == 49 && fn == 81)
    {
        /* Print stored QR */

        if (state->qr_data != NULL)
        {
            size_t i;

            if (!buffer_append_format(
                    state->xml,
                    "<symbol "
                    "type=\"qrcode_model_2\" "
                    "level=\"%s\" "
                    "width=\"%u\">",
                    state->qr_level,
                    state->qr_size))
            {
                return length;
            }

            for (i = 0; i < state->qr_length; i++)
            {
                if (!xml_escape_char(state->qr_data[i], state->xml))
                {
                    return length;
                }
            }

            if (!buffer_append_string(state->xml, "</symbol>"))
            {
                return length;
            }

            free(state->qr_data);
            state->qr_data = NULL;
            state->qr_length = 0;
        }
    }

    return end;
}


static size_t parse_raster(escpos_state_t *state, const uint8_t *data, size_t length, size_t index)
{
    size_t nbytes;
    size_t end;

    uint16_t width;
    uint16_t height;

    (void)state;

    if (!need(index, length, 8))
        return length;

    width =
        (uint16_t)data[index + 4] | ((uint16_t)data[index + 5] << 8);

    height = (uint16_t)data[index + 6] | ((uint16_t)data[index + 7] << 8);


    nbytes = (size_t)width * (size_t)height;

    end = index + 8 + nbytes;

    if (end > length)
        return length;

    return end;
}


bool escpos_parse(const void *input, size_t length, buffer_t *xml)
{
    escpos_state_t state;

    const uint8_t *data;
    size_t i;

    if (input == NULL || xml == NULL)
        return false;

    data = input;
    memset(&state, 0, sizeof(state));
    state.xml = xml;
    if (!buffer_init(&state.text))
        return false;

    reset_style(&state);

    i = 0;

    while (i < length)
    {
        switch (data[i])
        {
            case 0x0A:      /* LF */
                if (!flush_text(&state, true))
		{
			buffer_free(&state.text);
			free(state.qr_data);
			return false;
		}
                i++;
                break;

            case 0x0D:      /* CR */
                i++;
                break;

            case 0x1B:      /* ESC */
                i = parse_esc(&state, data, length, i);
                break;

            case 0x1D:      /* GS */
                i = parse_gs(
                        &state,
                        data,
                        length,
                        i);
                break;

            case 0x0C:      /* FF */
                i++;
                break;

            default:
                if (!append_text_byte(&state, data[i]))
                {
		    buffer_free(&state.text);
		    free(state.qr_data);
                    return false;
                }

                i++;
                break;
        }
    }

    if (!flush_text(&state, false))
    {
	buffer_free(&state.text);
	free(state.qr_data);
	return false;
    }

    buffer_free(&state.text);
    free(state.qr_data);

    return true;
}
