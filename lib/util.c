/* Various utilities
   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2007, 2009 Free Software Foundation, Inc.
   Written 1994, 1995, 1996 by:
   Miguel de Icaza, Janne Kukonlehto, Dugan Porter,
   Jakub Jelinek, Mauricio Plaza.

   The file_date routine is mostly from GNU's fileutils package,
   written by Richard Stallman and David MacKenzie.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file
 *  \brief Source: various utilities
 */

#include <config.h>

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/tty/win.h"        /* xterm_flag */
#include "lib/mcconfig.h"
#include "lib/fileloc.h"
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"

#include "src/filemanager/filegui.h"
#include "src/filemanager/file.h"       /* copy_file_file() */
#include "src/main.h"           /* eight_bit_clean */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define ismode(n,m) ((n & m) == m)

/* Number of attempts to create a temporary file */
#ifndef TMP_MAX
#define TMP_MAX 16384
#endif /* !TMP_MAX */

#define TMP_SUFFIX ".tmp"

#define ASCII_A (0x40 + 1)
#define ASCII_Z (0x40 + 26)
#define ASCII_a (0x60 + 1)
#define ASCII_z (0x60 + 26)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline int
is_7bit_printable (unsigned char c)
{
    return (c > 31 && c < 127);
}

/* --------------------------------------------------------------------------------------------- */

static inline int
is_iso_printable (unsigned char c)
{
    return ((c > 31 && c < 127) || c >= 160);
}

/* --------------------------------------------------------------------------------------------- */

static inline int
is_8bit_printable (unsigned char c)
{
    /* "Full 8 bits output" doesn't work on xterm */
    if (xterm_flag)
        return is_iso_printable (c);

    return (c > 31 && c != 127 && c != 155);
}

/* --------------------------------------------------------------------------------------------- */

static char *
resolve_symlinks (const char *path)
{
    char *buf, *buf2, *q, *r, c;
    int len;
    struct stat mybuf;
    const char *p;

    if (*path != PATH_SEP)
        return NULL;
    r = buf = g_malloc (MC_MAXPATHLEN);
    buf2 = g_malloc (MC_MAXPATHLEN);
    *r++ = PATH_SEP;
    *r = 0;
    p = path;
    for (;;)
    {
        q = strchr (p + 1, PATH_SEP);
        if (!q)
        {
            q = strchr (p + 1, 0);
            if (q == p + 1)
                break;
        }
        c = *q;
        *q = 0;
        if (mc_lstat (path, &mybuf) < 0)
        {
            g_free (buf);
            g_free (buf2);
            *q = c;
            return NULL;
        }
        if (!S_ISLNK (mybuf.st_mode))
            strcpy (r, p + 1);
        else
        {
            len = mc_readlink (path, buf2, MC_MAXPATHLEN - 1);
            if (len < 0)
            {
                g_free (buf);
                g_free (buf2);
                *q = c;
                return NULL;
            }
            buf2[len] = 0;
            if (*buf2 == PATH_SEP)
                strcpy (buf, buf2);
            else
                strcpy (r, buf2);
        }
        canonicalize_pathname (buf);
        r = strchr (buf, 0);
        if (!*r || *(r - 1) != PATH_SEP)
        {
            *r++ = PATH_SEP;
            *r = 0;
        }
        *q = c;
        p = q;
        if (!c)
            break;
    }
    if (!*buf)
        strcpy (buf, PATH_SEP_STR);
    else if (*(r - 1) == PATH_SEP && r != buf + 1)
        *(r - 1) = 0;
    g_free (buf2);
    return buf;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_util_write_backup_content (const char *from_file_name, const char *to_file_name)
{
    FILE *backup_fd;
    char *contents;
    gsize length;
    gboolean ret1 = TRUE;

    if (!g_file_get_contents (from_file_name, &contents, &length, NULL))
        return FALSE;

    backup_fd = fopen (to_file_name, "w");
    if (backup_fd == NULL)
    {
        g_free (contents);
        return FALSE;
    }

    if (fwrite ((const void *) contents, 1, length, backup_fd) != length)
        ret1 = FALSE;
    {
        int ret2;
        ret2 = fflush (backup_fd);
        ret2 = fclose (backup_fd);
    }
    g_free (contents);
    return ret1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
is_printable (int c)
{
    c &= 0xff;

#ifdef HAVE_CHARSET
    /* "Display bits" is ignored, since the user controls the output
       by setting the output codepage */
    return is_8bit_printable (c);
#else
    if (!eight_bit_clean)
        return is_7bit_printable (c);

    if (full_eight_bits)
    {
        return is_8bit_printable (c);
    }
    else
        return is_iso_printable (c);
#endif /* !HAVE_CHARSET */
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Quote the filename for the purpose of inserting it into the command
 * line.  If quote_percent is 1, replace "%" with "%%" - the percent is
 * processed by the mc command line.
 */
char *
name_quote (const char *s, int quote_percent)
{
    char *ret, *d;

    d = ret = g_malloc (strlen (s) * 2 + 2 + 1);
    if (*s == '-')
    {
        *d++ = '.';
        *d++ = '/';
    }

    for (; *s; s++, d++)
    {
        switch (*s)
        {
        case '%':
            if (quote_percent)
                *d++ = '%';
            break;
        case '\'':
        case '\\':
        case '\r':
        case '\n':
        case '\t':
        case '"':
        case ';':
        case ' ':
        case '?':
        case '|':
        case '[':
        case ']':
        case '{':
        case '}':
        case '<':
        case '>':
        case '`':
        case '!':
        case '$':
        case '&':
        case '*':
        case '(':
        case ')':
            *d++ = '\\';
            break;
        case '~':
        case '#':
            if (d == ret)
                *d++ = '\\';
            break;
        }
        *d = *s;
    }
    *d = '\0';
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

char *
fake_name_quote (const char *s, int quote_percent)
{
    (void) quote_percent;
    return g_strdup (s);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * path_trunc() is the same as str_trunc() but
 * it deletes possible password from path for security
 * reasons.
 */

const char *
path_trunc (const char *path, size_t trunc_len)
{
    char *secure_path = strip_password (g_strdup (path), 1);

    const char *ret = str_trunc (secure_path, trunc_len);
    g_free (secure_path);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

const char *
size_trunc (uintmax_t size, gboolean use_si)
{
    static char x[BUF_TINY];
    uintmax_t divisor = 1;
    const char *xtra = "";

    if (size > 999999999UL)
    {
        divisor = use_si ? 1000 : 1024;
        xtra = use_si ? "k" : "K";
        if (size / divisor > 999999999UL)
        {
            divisor = use_si ? (1000 * 1000) : (1024 * 1024);
            xtra = use_si ? "m" : "M";
        }
    }
    g_snprintf (x, sizeof (x), "%.0f%s", 1.0 * size / divisor, xtra);
    return x;
}

/* --------------------------------------------------------------------------------------------- */

const char *
size_trunc_sep (uintmax_t size, gboolean use_si)
{
    static char x[60];
    int count;
    const char *p, *y;
    char *d;

    p = y = size_trunc (size, use_si);
    p += strlen (p) - 1;
    d = x + sizeof (x) - 1;
    *d-- = '\0';
    while (p >= y && isalpha ((unsigned char) *p))
        *d-- = *p--;
    for (count = 0; p >= y; count++)
    {
        if (count == 3)
        {
            *d-- = ',';
            count = 0;
        }
        *d-- = *p--;
    }
    d++;
    if (*d == ',')
        d++;
    return d;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Print file SIZE to BUFFER, but don't exceed LEN characters,
 * not including trailing 0. BUFFER should be at least LEN+1 long.
 * This function is called for every file on panels, so avoid
 * floating point by any means.
 *
 * Units: size units (filesystem sizes are 1K blocks)
 *    0=bytes, 1=Kbytes, 2=Mbytes, etc.
 */

void
size_trunc_len (char *buffer, unsigned int len, uintmax_t size, int units, gboolean use_si)
{
    /* Avoid taking power for every file.  */
    static const uintmax_t power10[] = {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
    };
    static const char *const suffix[] = { "", "K", "M", "G", "T", "P", "E", "Z", "Y", NULL };
    static const char *const suffix_lc[] = { "", "k", "m", "g", "t", "p", "e", "z", "y", NULL };
    int j = 0;
    int size_remain;

    if (len == 0)
        len = 9;

    /*
     * recalculate from 1024 base to 1000 base if units>0
     * We can't just multiply by 1024 - that might cause overflow
     * if off_t type is too small
     */
    if (use_si)
        for (j = 0; j < units; j++)
        {
            size_remain = ((size % 125) * 1024) / 1000; /* size mod 125, recalculated */
            size = size / 125;  /* 128/125 = 1024/1000 */
            size = size * 128;  /* This will convert size from multiple of 1024 to multiple of 1000 */
            size += size_remain;        /* Re-add remainder lost by division/multiplication */
        }

    for (j = units; suffix[j] != NULL; j++)
    {
        if (size == 0)
        {
            if (j == units)
            {
                /* Empty files will print "0" even with minimal width.  */
                g_snprintf (buffer, len + 1, "0");
                break;
            }

            /* Use "~K" or just "K" if len is 1.  Use "B" for bytes.  */
            g_snprintf (buffer, len + 1, (len > 1) ? "~%s" : "%s",
                        (j > 1) ? (use_si ? suffix_lc[j - 1] : suffix[j - 1]) : "B");
            break;
        }

        if (size < power10[len - (j > 0)])
        {
            g_snprintf (buffer, len + 1, "%" PRIuMAX "%s", size, use_si ? suffix_lc[j] : suffix[j]);
            break;
        }

        /* Powers of 1000 or 1024, with rounding.  */
        if (use_si)
            size = (size + 500) / 1000;
        else
            size = (size + 512) >> 10;
    }
}

/* --------------------------------------------------------------------------------------------- */

const char *
string_perm (mode_t mode_bits)
{
    static char mode[11];

    strcpy (mode, "----------");
    if (S_ISDIR (mode_bits))
        mode[0] = 'd';
    if (S_ISCHR (mode_bits))
        mode[0] = 'c';
    if (S_ISBLK (mode_bits))
        mode[0] = 'b';
    if (S_ISLNK (mode_bits))
        mode[0] = 'l';
    if (S_ISFIFO (mode_bits))
        mode[0] = 'p';
    if (S_ISNAM (mode_bits))
        mode[0] = 'n';
    if (S_ISSOCK (mode_bits))
        mode[0] = 's';
    if (S_ISDOOR (mode_bits))
        mode[0] = 'D';
    if (ismode (mode_bits, S_IXOTH))
        mode[9] = 'x';
    if (ismode (mode_bits, S_IWOTH))
        mode[8] = 'w';
    if (ismode (mode_bits, S_IROTH))
        mode[7] = 'r';
    if (ismode (mode_bits, S_IXGRP))
        mode[6] = 'x';
    if (ismode (mode_bits, S_IWGRP))
        mode[5] = 'w';
    if (ismode (mode_bits, S_IRGRP))
        mode[4] = 'r';
    if (ismode (mode_bits, S_IXUSR))
        mode[3] = 'x';
    if (ismode (mode_bits, S_IWUSR))
        mode[2] = 'w';
    if (ismode (mode_bits, S_IRUSR))
        mode[1] = 'r';
#ifdef S_ISUID
    if (ismode (mode_bits, S_ISUID))
        mode[3] = (mode[3] == 'x') ? 's' : 'S';
#endif /* S_ISUID */
#ifdef S_ISGID
    if (ismode (mode_bits, S_ISGID))
        mode[6] = (mode[6] == 'x') ? 's' : 'S';
#endif /* S_ISGID */
#ifdef S_ISVTX
    if (ismode (mode_bits, S_ISVTX))
        mode[9] = (mode[9] == 'x') ? 't' : 'T';
#endif /* S_ISVTX */
    return mode;
}

/* --------------------------------------------------------------------------------------------- */
/**
 *  p: string which might contain an url with a password (this parameter is
 *  modified in place).
 *  has_prefix = 0: The first parameter is an url without a prefix
 *  (user[:pass]@]machine[:port][remote-dir). Delete
 *  the password.
 *  has_prefix = 1: Search p for known url prefixes. If found delete
 *  the password from the url.
 *  Caveat: only the first url is found
 */

char *
strip_password (char *p, int has_prefix)
{
    static const struct
    {
        const char *name;
        size_t len;
    } prefixes[] =
    {
        /* *INDENT-OFF* */
        { "/#ftp:", 6 },
        { "ftp://", 6 },
        { "/#smb:", 6 },
        { "smb://", 6 },
        { "/#sh:", 5 },
        { "sh://", 5 },
        { "ssh://", 6 }
        /* *INDENT-ON* */
    };

    char *at, *inner_colon, *dir;
    size_t i;
    char *result = p;

    for (i = 0; i < sizeof (prefixes) / sizeof (prefixes[0]); i++)
    {
        char *q;

        if (has_prefix)
        {
            q = strstr (p, prefixes[i].name);
            if (q == NULL)
                continue;
            else
                p = q + prefixes[i].len;
        }

        dir = strchr (p, PATH_SEP);
        if (dir != NULL)
            *dir = '\0';

        /* search for any possible user */
        at = strrchr (p, '@');

        if (dir)
            *dir = PATH_SEP;

        /* We have a username */
        if (at)
        {
            inner_colon = memchr (p, ':', at - p);
            if (inner_colon)
                memmove (inner_colon, at, strlen (at) + 1);
        }
        break;
    }
    return (result);
}

/* --------------------------------------------------------------------------------------------- */

const char *
strip_home_and_password (const char *dir)
{
    size_t len;
    static char newdir[MC_MAXPATHLEN];

    len = strlen (mc_config_get_home_dir ());
    if (mc_config_get_home_dir () != NULL && strncmp (dir, mc_config_get_home_dir (), len) == 0 &&
        (dir[len] == PATH_SEP || dir[len] == '\0'))
    {
        newdir[0] = '~';
        g_strlcpy (&newdir[1], &dir[len], sizeof (newdir) - 1);
        return newdir;
    }

    /* We do not strip homes in /#ftp tree, I do not like ~'s there 
       (see ftpfs.c why) */
    g_strlcpy (newdir, dir, sizeof (newdir));
    strip_password (newdir, 1);
    return newdir;
}

/* --------------------------------------------------------------------------------------------- */

const char *
extension (const char *filename)
{
    const char *d = strrchr (filename, '.');
    return (d != NULL) ? d + 1 : "";
}

/* --------------------------------------------------------------------------------------------- */

int
check_for_default (const char *default_file, const char *file)
{
    if (!exist_file (file))
    {
        FileOpContext *ctx;
        FileOpTotalContext *tctx;

        if (!exist_file (default_file))
            return -1;

        ctx = file_op_context_new (OP_COPY);
        tctx = file_op_total_context_new ();
        file_op_context_create_ui (ctx, 0, FALSE);
        copy_file_file (tctx, ctx, default_file, file);
        file_op_total_context_destroy (tctx);
        file_op_context_destroy (ctx);
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

char *
load_mc_home_file (const char *from, const char *filename, char **allocated_filename)
{
    char *hintfile_base, *hintfile;
    char *lang;
    char *data;

    hintfile_base = g_build_filename (from, filename, (char *) NULL);
    lang = guess_message_value ();

    hintfile = g_strconcat (hintfile_base, ".", lang, (char *) NULL);
    if (!g_file_get_contents (hintfile, &data, NULL, NULL))
    {
        /* Fall back to the two-letter language code */
        if (lang[0] != '\0' && lang[1] != '\0')
            lang[2] = '\0';
        g_free (hintfile);
        hintfile = g_strconcat (hintfile_base, ".", lang, (char *) NULL);
        if (!g_file_get_contents (hintfile, &data, NULL, NULL))
        {
            g_free (hintfile);
            hintfile = hintfile_base;
            g_file_get_contents (hintfile_base, &data, NULL, NULL);
        }
    }

    g_free (lang);

    if (hintfile != hintfile_base)
        g_free (hintfile_base);

    if (allocated_filename != NULL)
        *allocated_filename = hintfile;
    else
        g_free (hintfile);

    return data;
}

/* --------------------------------------------------------------------------------------------- */

const char *
extract_line (const char *s, const char *top)
{
    static char tmp_line[BUF_MEDIUM];
    char *t = tmp_line;

    while (*s && *s != '\n' && (size_t) (t - tmp_line) < sizeof (tmp_line) - 1 && s < top)
        *t++ = *s++;
    *t = 0;
    return tmp_line;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * The basename routine
 */

const char *
x_basename (const char *s)
{
    const char *where;
    return ((where = strrchr (s, PATH_SEP))) ? where + 1 : s;
}

/* --------------------------------------------------------------------------------------------- */

const char *
unix_error_string (int error_num)
{
    static char buffer[BUF_LARGE];
    gchar *strerror_currentlocale;

    strerror_currentlocale = g_locale_from_utf8 (g_strerror (error_num), -1, NULL, NULL, NULL);
    g_snprintf (buffer, sizeof (buffer), "%s (%d)", strerror_currentlocale, error_num);
    g_free (strerror_currentlocale);

    return buffer;
}

/* --------------------------------------------------------------------------------------------- */

const char *
skip_separators (const char *s)
{
    const char *su = s;

    for (; *su; str_cnext_char (&su))
        if (*su != ' ' && *su != '\t' && *su != ',')
            break;

    return su;
}

/* --------------------------------------------------------------------------------------------- */

const char *
skip_numbers (const char *s)
{
    const char *su = s;

    for (; *su; str_cnext_char (&su))
        if (!str_isdigit (su))
            break;

    return su;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Remove all control sequences from the argument string.  We define
 * "control sequence", in a sort of pidgin BNF, as follows:
 *
 * control-seq = Esc non-'['
 *             | Esc '[' (0 or more digits or ';' or '?') (any other char)
 *
 * This scheme works for all the terminals described in my termcap /
 * terminfo databases, except the Hewlett-Packard 70092 and some Wyse
 * terminals.  If I hear from a single person who uses such a terminal
 * with MC, I'll be glad to add support for it.  (Dugan)
 * Non-printable characters are also removed.
 */

char *
strip_ctrl_codes (char *s)
{
    char *w;                    /* Current position where the stripped data is written */
    char *r;                    /* Current position where the original data is read */
    char *n;

    if (!s)
        return 0;

    for (w = s, r = s; *r;)
    {
        if (*r == ESC_CHAR)
        {
            /* Skip the control sequence's arguments */ ;
            /* '(' need to avoid strange 'B' letter in *Suse (if mc runs under root user) */
            if (*(++r) == '[' || *r == '(')
            {
                /* strchr() matches trailing binary 0 */
                while (*(++r) && strchr ("0123456789;?", *r));
            }
            else if (*r == ']')
            {
                /*
                 * Skip xterm's OSC (Operating System Command)
                 * http://www.xfree86.org/current/ctlseqs.html
                 * OSC P s ; P t ST
                 * OSC P s ; P t BEL
                 */
                char *new_r = r;

                for (; *new_r; ++new_r)
                {
                    switch (*new_r)
                    {
                        /* BEL */
                    case '\a':
                        r = new_r;
                        goto osc_out;
                    case ESC_CHAR:
                        /* ST */
                        if (*(new_r + 1) == '\\')
                        {
                            r = new_r + 1;
                            goto osc_out;
                        }
                    }
                }
              osc_out:;
            }

            /*
             * Now we are at the last character of the sequence.
             * Skip it unless it's binary 0.
             */
            if (*r)
                r++;
            continue;
        }

        n = str_get_next_char (r);
        if (str_isprint (r))
        {
            memmove (w, r, n - r);
            w += n - r;
        }
        r = n;
    }
    *w = 0;
    return s;
}

/* --------------------------------------------------------------------------------------------- */

enum compression_type
get_compression_type (int fd, const char *name)
{
    unsigned char magic[16];
    size_t str_len;

    /* Read the magic signature */
    if (mc_read (fd, (char *) magic, 4) != 4)
        return COMPRESSION_NONE;

    /* GZIP_MAGIC and OLD_GZIP_MAGIC */
    if (magic[0] == 037 && (magic[1] == 0213 || magic[1] == 0236))
    {
        return COMPRESSION_GZIP;
    }

    /* PKZIP_MAGIC */
    if (magic[0] == 0120 && magic[1] == 0113 && magic[2] == 003 && magic[3] == 004)
    {
        /* Read compression type */
        mc_lseek (fd, 8, SEEK_SET);
        if (mc_read (fd, (char *) magic, 2) != 2)
            return COMPRESSION_NONE;

        /* Gzip can handle only deflated (8) or stored (0) files */
        if ((magic[0] != 8 && magic[0] != 0) || magic[1] != 0)
            return COMPRESSION_NONE;

        /* Compatible with gzip */
        return COMPRESSION_GZIP;
    }

    /* PACK_MAGIC and LZH_MAGIC and compress magic */
    if (magic[0] == 037 && (magic[1] == 036 || magic[1] == 0240 || magic[1] == 0235))
    {
        /* Compatible with gzip */
        return COMPRESSION_GZIP;
    }

    /* BZIP and BZIP2 files */
    if ((magic[0] == 'B') && (magic[1] == 'Z') && (magic[3] >= '1') && (magic[3] <= '9'))
    {
        switch (magic[2])
        {
        case '0':
            return COMPRESSION_BZIP;
        case 'h':
            return COMPRESSION_BZIP2;
        }
    }

    /* Support for LZMA (only utils format with magic in header).
     * This is the default format of LZMA utils 4.32.1 and later. */

    if (mc_read (fd, (char *) magic + 4, 2) != 2)
        return COMPRESSION_NONE;

    /* LZMA utils format */
    if (magic[0] == 0xFF
        && magic[1] == 'L'
        && magic[2] == 'Z' && magic[3] == 'M' && magic[4] == 'A' && magic[5] == 0x00)
        return COMPRESSION_LZMA;

    /* XZ compression magic */
    if (magic[0] == 0xFD
        && magic[1] == 0x37
        && magic[2] == 0x7A && magic[3] == 0x58 && magic[4] == 0x5A && magic[5] == 0x00)
        return COMPRESSION_XZ;

    str_len = strlen (name);
    /* HACK: we must belive to extention of LZMA file :) ... */
    if ((str_len > 5 && strcmp (&name[str_len - 5], ".lzma") == 0) ||
        (str_len > 4 && strcmp (&name[str_len - 4], ".tlz") == 0))
        return COMPRESSION_LZMA;

    return COMPRESSION_NONE;
}

/* --------------------------------------------------------------------------------------------- */

const char *
decompress_extension (int type)
{
    switch (type)
    {
    case COMPRESSION_GZIP:
        return "#ugz";
    case COMPRESSION_BZIP:
        return "#ubz";
    case COMPRESSION_BZIP2:
        return "#ubz2";
    case COMPRESSION_LZMA:
        return "#ulzma";
    case COMPRESSION_XZ:
        return "#uxz";
    }
    /* Should never reach this place */
    fprintf (stderr, "Fatal: decompress_extension called with an unknown argument\n");
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

void
wipe_password (char *passwd)
{
    char *p = passwd;

    if (!p)
        return;
    for (; *p; p++)
        *p = 0;
    g_free (passwd);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Convert "\E" -> esc character and ^x to control-x key and ^^ to ^ key
 * @returns a newly allocated string
 */

char *
convert_controls (const char *p)
{
    char *valcopy = g_strdup (p);
    char *q;

    /* Parse the escape special character */
    for (q = valcopy; *p;)
    {
        if (*p == '\\')
        {
            p++;
            if ((*p == 'e') || (*p == 'E'))
            {
                p++;
                *q++ = ESC_CHAR;
            }
        }
        else
        {
            if (*p == '^')
            {
                p++;
                if (*p == '^')
                    *q++ = *p++;
                else
                {
                    char c = (*p | 0x20);
                    if (c >= 'a' && c <= 'z')
                    {
                        *q++ = c - 'a' + 1;
                        p++;
                    }
                    else if (*p)
                        p++;
                }
            }
            else
                *q++ = *p++;
        }
    }
    *q = 0;
    return valcopy;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Finds out a relative path from first to second, i.e. goes as many ..
 * as needed up in first and then goes down using second
 */

char *
diff_two_paths (const char *first, const char *second)
{
    char *p, *q, *r, *s, *buf = NULL;
    int i, j, prevlen = -1, currlen;
    char *my_first = NULL, *my_second = NULL;

    my_first = resolve_symlinks (first);
    if (my_first == NULL)
        return NULL;
    my_second = resolve_symlinks (second);
    if (my_second == NULL)
    {
        g_free (my_first);
        return NULL;
    }
    for (j = 0; j < 2; j++)
    {
        p = my_first;
        q = my_second;
        for (;;)
        {
            r = strchr (p, PATH_SEP);
            s = strchr (q, PATH_SEP);
            if (!r || !s)
                break;
            *r = 0;
            *s = 0;
            if (strcmp (p, q))
            {
                *r = PATH_SEP;
                *s = PATH_SEP;
                break;
            }
            else
            {
                *r = PATH_SEP;
                *s = PATH_SEP;
            }
            p = r + 1;
            q = s + 1;
        }
        p--;
        for (i = 0; (p = strchr (p + 1, PATH_SEP)) != NULL; i++);
        currlen = (i + 1) * 3 + strlen (q) + 1;
        if (j)
        {
            if (currlen < prevlen)
                g_free (buf);
            else
            {
                g_free (my_first);
                g_free (my_second);
                return buf;
            }
        }
        p = buf = g_malloc (currlen);
        prevlen = currlen;
        for (; i >= 0; i--, p += 3)
            strcpy (p, "../");
        strcpy (p, q);
    }
    g_free (my_first);
    g_free (my_second);
    return buf;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * If filename is NULL, then we just append PATH_SEP to the dir
 */

char *
concat_dir_and_file (const char *dir, const char *file)
{
    int i = strlen (dir);

    if (dir[i - 1] == PATH_SEP)
        return g_strconcat (dir, file, (char *) NULL);
    else
        return g_strconcat (dir, PATH_SEP_STR, file, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Append text to GList, remove all entries with the same text
 */

GList *
list_append_unique (GList * list, char *text)
{
    GList *lc_link;

    /*
     * Go to the last position and traverse the list backwards
     * starting from the second last entry to make sure that we
     * are not removing the current link.
     */
    list = g_list_append (list, text);
    list = g_list_last (list);
    lc_link = g_list_previous (list);

    while (lc_link != NULL)
    {
        GList *newlink;

        newlink = g_list_previous (lc_link);
        if (strcmp ((char *) lc_link->data, text) == 0)
        {
            GList *tmp;

            g_free (lc_link->data);
            tmp = g_list_remove_link (list, lc_link);
            g_list_free_1 (lc_link);
        }
        lc_link = newlink;
    }

    return list;
}

/* --------------------------------------------------------------------------------------------- */
/* Following code heavily borrows from libiberty, mkstemps.c */
/*
 * Arguments:
 * pname (output) - pointer to the name of the temp file (needs g_free).
 *                  NULL if the function fails.
 * prefix - part of the filename before the random part.
 *          Prepend $TMPDIR or /tmp if there are no path separators.
 * suffix - if not NULL, part of the filename after the random part.
 *
 * Result:
 * handle of the open file or -1 if couldn't open any.
 */

int
mc_mkstemps (char **pname, const char *prefix, const char *suffix)
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static unsigned long value;
    struct timeval tv;
    char *tmpbase;
    char *tmpname;
    char *XXXXXX;
    int count;

    if (strchr (prefix, PATH_SEP) == NULL)
    {
        /* Add prefix first to find the position of XXXXXX */
        tmpbase = concat_dir_and_file (mc_tmpdir (), prefix);
    }
    else
    {
        tmpbase = g_strdup (prefix);
    }

    tmpname = g_strconcat (tmpbase, "XXXXXX", suffix, (char *) NULL);
    *pname = tmpname;
    XXXXXX = &tmpname[strlen (tmpbase)];
    g_free (tmpbase);

    /* Get some more or less random data.  */
    gettimeofday (&tv, NULL);
    value += (tv.tv_usec << 16) ^ tv.tv_sec ^ getpid ();

    for (count = 0; count < TMP_MAX; ++count)
    {
        unsigned long v = value;
        int fd;

        /* Fill in the random bits.  */
        XXXXXX[0] = letters[v % 62];
        v /= 62;
        XXXXXX[1] = letters[v % 62];
        v /= 62;
        XXXXXX[2] = letters[v % 62];
        v /= 62;
        XXXXXX[3] = letters[v % 62];
        v /= 62;
        XXXXXX[4] = letters[v % 62];
        v /= 62;
        XXXXXX[5] = letters[v % 62];

        fd = open (tmpname, O_RDWR | O_CREAT | O_TRUNC | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd >= 0)
        {
            /* Successfully created.  */
            return fd;
        }

        /* This is a random value.  It is only necessary that the next
           TMP_MAX values generated by adding 7777 to VALUE are different
           with (module 2^32).  */
        value += 7777;
    }

    /* Unsuccessful. Free the filename. */
    g_free (tmpname);
    *pname = NULL;

    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Read and restore position for the given filename.
 * If there is no stored data, return line 1 and col 0.
 */

void
load_file_position (const char *filename, long *line, long *column, off_t * offset,
                    GArray ** bookmarks)
{
    char *fn;
    FILE *f;
    char buf[MC_MAXPATHLEN + 100];
    const size_t len = strlen (filename);

    /* defaults */
    *line = 1;
    *column = 0;
    *offset = 0;

    /* open file with positions */
    fn = g_build_filename (mc_config_get_cache_path (), MC_FILEPOS_FILE, NULL);
    f = fopen (fn, "r");
    g_free (fn);
    if (f == NULL)
        return;

    /* prepare array for serialized bookmarks */
    *bookmarks = g_array_sized_new (FALSE, FALSE, sizeof (size_t), MAX_SAVED_BOOKMARKS);

    while (fgets (buf, sizeof (buf), f) != NULL)
    {
        const char *p;
        gchar **pos_tokens;

        /* check if the filename matches the beginning of string */
        if (strncmp (buf, filename, len) != 0)
            continue;

        /* followed by single space */
        if (buf[len] != ' ')
            continue;

        /* and string without spaces */
        p = &buf[len + 1];
        if (strchr (p, ' ') != NULL)
            continue;

        pos_tokens = g_strsplit (p, ";", 3 + MAX_SAVED_BOOKMARKS);
        if (pos_tokens[0] == NULL)
        {
            *line = 1;
            *column = 0;
            *offset = 0;
        }
        else
        {
            *line = strtol (pos_tokens[0], NULL, 10);
            if (pos_tokens[1] == NULL)
            {
                *column = 0;
                *offset = 0;
            }
            else
            {
                *column = strtol (pos_tokens[1], NULL, 10);
                if (pos_tokens[2] == NULL)
                    *offset = 0;
                else
                {
                    size_t i;

                    *offset = strtoll (pos_tokens[2], NULL, 10);

                    for (i = 0; i < MAX_SAVED_BOOKMARKS && pos_tokens[3 + i] != NULL; i++)
                    {
                        size_t val;

                        val = strtoul (pos_tokens[3 + i], NULL, 10);
                        g_array_append_val (*bookmarks, val);
                    }
                }
            }
        }

        g_strfreev (pos_tokens);
    }

    fclose (f);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Save position for the given file
 */

void
save_file_position (const char *filename, long line, long column, off_t offset, GArray * bookmarks)
{
    static size_t filepos_max_saved_entries = 0;
    char *fn, *tmp_fn;
    FILE *f, *tmp_f;
    char buf[MC_MAXPATHLEN + 100];
    size_t i;
    const size_t len = strlen (filename);
    gboolean src_error = FALSE;

    if (filepos_max_saved_entries == 0)
        filepos_max_saved_entries = mc_config_get_int (mc_main_config, CONFIG_APP_SECTION,
                                                       "filepos_max_saved_entries", 1024);

    fn = g_build_filename (mc_config_get_cache_path (), MC_FILEPOS_FILE, NULL);
    if (fn == NULL)
        goto early_error;

    mc_util_make_backup_if_possible (fn, TMP_SUFFIX);

    /* open file */
    f = fopen (fn, "w");
    if (f == NULL)
        goto open_target_error;

    tmp_fn = g_strdup_printf ("%s" TMP_SUFFIX, fn);
    tmp_f = fopen (tmp_fn, "r");
    if (tmp_f == NULL)
    {
        src_error = TRUE;
        goto open_source_error;
    }

    /* put the new record */
    if (line != 1 || column != 0 || bookmarks != NULL)
    {
        if (fprintf (f, "%s %ld;%ld;%" PRIuMAX, filename, line, column, (uintmax_t) offset) < 0)
            goto write_position_error;
        if (bookmarks != NULL)
            for (i = 0; i < bookmarks->len && i < MAX_SAVED_BOOKMARKS; i++)
                if (fprintf (f, ";%zu", g_array_index (bookmarks, size_t, i)) < 0)
                    goto write_position_error;

        if (fprintf (f, "\n") < 0)
            goto write_position_error;
    }

    i = 1;
    while (fgets (buf, sizeof (buf), tmp_f) != NULL)
    {
        if (buf[len] == ' ' && strncmp (buf, filename, len) == 0
            && strchr (&buf[len + 1], ' ') == NULL)
            continue;

        fprintf (f, "%s", buf);
        if (++i > filepos_max_saved_entries)
            break;
    }

  write_position_error:
    fclose (tmp_f);
  open_source_error:
    g_free (tmp_fn);
    fclose (f);
    if (src_error)
        mc_util_restore_from_backup_if_possible (fn, TMP_SUFFIX);
    else
        mc_util_unlink_backup_if_possible (fn, TMP_SUFFIX);
  open_target_error:
    g_free (fn);
  early_error:
    if (bookmarks != NULL)
        g_array_free (bookmarks, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

extern int
ascii_alpha_to_cntrl (int ch)
{
    if ((ch >= ASCII_A && ch <= ASCII_Z) || (ch >= ASCII_a && ch <= ASCII_z))
    {
        ch &= 0x1f;
    }
    return ch;
}

/* --------------------------------------------------------------------------------------------- */

const char *
Q_ (const char *s)
{
    const char *result, *sep;

    result = _(s);
    sep = strchr (result, '|');
    return (sep != NULL) ? sep + 1 : result;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_util_make_backup_if_possible (const char *file_name, const char *backup_suffix)
{
    struct stat stat_buf;
    char *backup_path;
    gboolean ret;
    if (!exist_file (file_name))
        return FALSE;

    backup_path = g_strdup_printf ("%s%s", file_name, backup_suffix);

    if (backup_path == NULL)
        return FALSE;

    ret = mc_util_write_backup_content (file_name, backup_path);

    if (ret)
    {
        /* Backup file will have same ownership with main file. */
        if (stat (file_name, &stat_buf) == 0)
            chmod (backup_path, stat_buf.st_mode);
        else
            chmod (backup_path, S_IRUSR | S_IWUSR);
    }

    g_free (backup_path);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_util_restore_from_backup_if_possible (const char *file_name, const char *backup_suffix)
{
    gboolean ret;
    char *backup_path;

    backup_path = g_strdup_printf ("%s%s", file_name, backup_suffix);
    if (backup_path == NULL)
        return FALSE;

    ret = mc_util_write_backup_content (backup_path, file_name);
    g_free (backup_path);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_util_unlink_backup_if_possible (const char *file_name, const char *backup_suffix)
{
    char *backup_path;

    backup_path = g_strdup_printf ("%s%s", file_name, backup_suffix);
    if (backup_path == NULL)
        return FALSE;

    if (exist_file (backup_path))
        mc_unlink (backup_path);

    g_free (backup_path);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * partly taken from dcigettext.c, returns "" for default locale
 * value should be freed by calling function g_free()
 */

char *
guess_message_value (void)
{
    static const char *const var[] = {
        /* Setting of LC_ALL overwrites all other.  */
        /* Do not use LANGUAGE for check user locale and drowing hints */
        "LC_ALL",
        /* Next comes the name of the desired category.  */
        "LC_MESSAGES",
        /* Last possibility is the LANG environment variable.  */
        "LANG",
        /* NULL exit loops */
        NULL
    };

    unsigned i = 0;
    const char *locale = NULL;

    while (var[i] != NULL)
    {
        locale = getenv (var[i]);
        if (locale != NULL && locale[0] != '\0')
            break;
        i++;
    }

    if (locale == NULL)
        locale = "";

    return g_strdup (locale);
}

/* --------------------------------------------------------------------------------------------- */
