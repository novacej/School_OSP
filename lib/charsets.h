/** \file charsets.h
 *  \brief Header: Text conversion from one charset to another
 */

#ifndef MC__CHARSETS_H
#define MC__CHARSETS_H

#include "lib/global.h"


/*** typedefs(not structures) and defined constants **********************************************/

#ifdef HAVE_CHARSET
#define UNKNCHAR '\001'
#else
#define convert_to_display_c(c) (c)
#define convert_from_input_c(c) (c)
#define convert_to_display(str) do {} while (0)
#define convert_from_input(str) do {} while (0)
#endif /* HAVE_CHARSET */

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/
#ifdef HAVE_CHARSET

typedef struct
{
    char *id;
    char *name;
} codepage_desc;

/*** global variables defined in .c file *********************************************************/

extern unsigned char conv_displ[256];
extern unsigned char conv_input[256];

extern const char *cp_display;
extern const char *cp_source;
extern GPtrArray *codepages;

/*** declarations of public functions ************************************************************/

const char *get_codepage_id (const int n);
int get_codepage_index (const char *id);
void load_codepages_list (void);
void free_codepages_list (void);
gboolean is_supported_encoding (const char *encoding);
char *init_translation_table (int cpsource, int cpdisplay);
void convert_to_display (char *str);
void convert_from_input (char *str);
void convert_string (unsigned char *str);

/*
 * Converter from utf to selected codepage
 * param str, utf char
 * return char in needle codepage (by global int source_codepage)
 */
unsigned char convert_from_utf_to_current (const char *str);

/*
 * Converter from utf to selected codepage
 * param input_char, gunichar
 * return char in needle codepage (by global int source_codepage)
 */
unsigned char convert_from_utf_to_current_c (const int input_char, GIConv conv);

/*
 * Converter from selected codepage 8-bit
 * param char input_char, GIConv converter
 * return int utf char
 */
int convert_from_8bit_to_utf_c (const char input_char, GIConv conv);

/*
 * Converter from display codepage 8-bit to utf-8
 * param char input_char, GIConv converter
 * return int utf char
 */
int convert_from_8bit_to_utf_c2 (const char input_char);

GString *str_convert_to_input (char *str);
GString *str_nconvert_to_input (char *str, int len);

GString *str_convert_to_display (char *str);
GString *str_nconvert_to_display (char *str, int len);

/*** inline functions ****************************************************************************/

/* Convert single characters */
static inline int
convert_to_display_c (int c)
{
    if (c < 0 || c >= 256)
        return c;
    return conv_displ[c];
}

static inline int
convert_from_input_c (int c)
{
    if (c < 0 || c >= 256)
        return c;
    return conv_input[c];
}

#endif /* HAVE_CHARSET */

#endif /* MC__CHARSETS_H */
