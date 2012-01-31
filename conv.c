#include "conv.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef USE_ICONV
#include <iconv.h>
#endif
#ifdef WIN3264
#include <windows.h>
#endif

/*
 * Canonical encoding names and their properties.
 * "iso-8859-n" is handled by enc_canonize() directly.
 */
static struct {
    const char *name;
    int prop;
    int codepage;
} enc_canon_table[] = {
#define IDX_LATIN_1     0
    {"latin1",          ENC_8BIT + ENC_LATIN1,  1252},
#define IDX_ISO_2       1
    {"iso-8859-2",      ENC_8BIT,               0},
#define IDX_ISO_3       2
    {"iso-8859-3",      ENC_8BIT,               0},
#define IDX_ISO_4       3
    {"iso-8859-4",      ENC_8BIT,               0},
#define IDX_ISO_5       4
    {"iso-8859-5",      ENC_8BIT,               0},
#define IDX_ISO_6       5
    {"iso-8859-6",      ENC_8BIT,               0},
#define IDX_ISO_7       6
    {"iso-8859-7",      ENC_8BIT,               0},
#define IDX_ISO_8       7
    {"iso-8859-8",      ENC_8BIT,               0},
#define IDX_ISO_9       8
    {"iso-8859-9",      ENC_8BIT,               0},
#define IDX_ISO_10      9
    {"iso-8859-10",     ENC_8BIT,               0},
#define IDX_ISO_11      10
    {"iso-8859-11",     ENC_8BIT,               0},
#define IDX_ISO_13      11
    {"iso-8859-13",     ENC_8BIT,               0},
#define IDX_ISO_14      12
    {"iso-8859-14",     ENC_8BIT,               0},
#define IDX_ISO_15      13
    {"iso-8859-15",     ENC_8BIT + ENC_LATIN9,  0},
#define IDX_KOI8_R      14
    {"koi8-r",          ENC_8BIT,               0},
#define IDX_KOI8_U      15
    {"koi8-u",          ENC_8BIT,               0},
#define IDX_UTF8        16
    {"utf-8",           ENC_UNICODE,            0},
#define IDX_UCS2        17
    {"ucs-2",           ENC_UNICODE + ENC_ENDIAN_B + ENC_2BYTE, 0},
#define IDX_UCS2LE      18
    {"ucs-2le",         ENC_UNICODE + ENC_ENDIAN_L + ENC_2BYTE, 0},
#define IDX_UTF16       19
    {"utf-16",          ENC_UNICODE + ENC_ENDIAN_B + ENC_2WORD, 0},
#define IDX_UTF16LE     20
    {"utf-16le",        ENC_UNICODE + ENC_ENDIAN_L + ENC_2WORD, 0},
#define IDX_UCS4        21
    {"ucs-4",           ENC_UNICODE + ENC_ENDIAN_B + ENC_4BYTE, 0},
#define IDX_UCS4LE      22
    {"ucs-4le",         ENC_UNICODE + ENC_ENDIAN_L + ENC_4BYTE, 0},

    /* For debugging DBCS encoding on Unix. */
#define IDX_DEBUG       23
    {"debug",           ENC_DBCS,               DBCS_DEBUG},
#define IDX_EUC_JP      24
    {"euc-jp",          ENC_DBCS,               DBCS_JPNU},
#define IDX_SJIS        25
    {"sjis",            ENC_DBCS,               DBCS_JPN},
#define IDX_EUC_KR      26
    {"euc-kr",          ENC_DBCS,               DBCS_KORU},
#define IDX_EUC_CN      27
    {"euc-cn",          ENC_DBCS,               DBCS_CHSU},
#define IDX_EUC_TW      28
    {"euc-tw",          ENC_DBCS,               DBCS_CHTU},
#define IDX_BIG5        29
    {"big5",            ENC_DBCS,               DBCS_CHT},

    /* MS-DOS and MS-Windows codepages are included here, so that they can be
     * used on Unix too.  Most of them are similar to ISO-8859 encodings, but
     * not exactly the same. */
#define IDX_CP437       30
    {"cp437",           ENC_8BIT,               437}, /* like iso-8859-1 */
#define IDX_CP737       31
    {"cp737",           ENC_8BIT,               737}, /* like iso-8859-7 */
#define IDX_CP775       32
    {"cp775",           ENC_8BIT,               775}, /* Baltic */
#define IDX_CP850       33
    {"cp850",           ENC_8BIT,               850}, /* like iso-8859-4 */
#define IDX_CP852       34
    {"cp852",           ENC_8BIT,               852}, /* like iso-8859-1 */
#define IDX_CP855       35
    {"cp855",           ENC_8BIT,               855}, /* like iso-8859-2 */
#define IDX_CP857       36
    {"cp857",           ENC_8BIT,               857}, /* like iso-8859-5 */
#define IDX_CP860       37
    {"cp860",           ENC_8BIT,               860}, /* like iso-8859-9 */
#define IDX_CP861       38
    {"cp861",           ENC_8BIT,               861}, /* like iso-8859-1 */
#define IDX_CP862       39
    {"cp862",           ENC_8BIT,               862}, /* like iso-8859-1 */
#define IDX_CP863       40
    {"cp863",           ENC_8BIT,               863}, /* like iso-8859-8 */
#define IDX_CP865       41
    {"cp865",           ENC_8BIT,               865}, /* like iso-8859-1 */
#define IDX_CP866       42
    {"cp866",           ENC_8BIT,               866}, /* like iso-8859-5 */
#define IDX_CP869       43
    {"cp869",           ENC_8BIT,               869}, /* like iso-8859-7 */
#define IDX_CP874       44
    {"cp874",           ENC_8BIT,               874}, /* Thai */
#define IDX_CP932       45
    {"cp932",           ENC_DBCS,               DBCS_JPN},
#define IDX_CP936       46
    {"cp936",           ENC_DBCS,               DBCS_CHS},
#define IDX_CP949       47
    {"cp949",           ENC_DBCS,               DBCS_KOR},
#define IDX_CP950       48
    {"cp950",           ENC_DBCS,               DBCS_CHT},
#define IDX_CP1250      49
    {"cp1250",          ENC_8BIT,               1250}, /* Czech, Polish, etc. */
#define IDX_CP1251      50
    {"cp1251",          ENC_8BIT,               1251}, /* Cyrillic */
    /* cp1252 is considered to be equal to latin1 */
#define IDX_CP1253      51
    {"cp1253",          ENC_8BIT,               1253}, /* Greek */
#define IDX_CP1254      52
    {"cp1254",          ENC_8BIT,               1254}, /* Turkish */
#define IDX_CP1255      53
    {"cp1255",          ENC_8BIT,               1255}, /* Hebrew */
#define IDX_CP1256      54
    {"cp1256",          ENC_8BIT,               1256}, /* Arabic */
#define IDX_CP1257      55
    {"cp1257",          ENC_8BIT,               1257}, /* Baltic */
#define IDX_CP1258      56
    {"cp1258",          ENC_8BIT,               1258}, /* Vietnamese */

#define IDX_MACROMAN    57
    {"macroman",        ENC_8BIT + ENC_MACROMAN, 0},        /* Mac OS */
#define IDX_DECMCS      58
    {"dec-mcs",         ENC_8BIT,                0},        /* DEC MCS */
#define IDX_HPROMAN8    59
    {"hp-roman8",       ENC_8BIT,                0},        /* HP Roman8 */
#define IDX_COUNT       60
};

/*
 * Aliases for encoding names.
 */
static struct {
    const char *name;
    int canon;
} enc_alias_table[] = {
    {"ansi",            IDX_LATIN_1},
    {"iso-8859-1",      IDX_LATIN_1},
    {"latin2",          IDX_ISO_2},
    {"latin3",          IDX_ISO_3},
    {"latin4",          IDX_ISO_4},
    {"cyrillic",        IDX_ISO_5},
    {"arabic",          IDX_ISO_6},
    {"greek",           IDX_ISO_7},
#ifdef WIN3264
    {"hebrew",          IDX_CP1255},
#else
    {"hebrew",          IDX_ISO_8},
#endif
    {"latin5",          IDX_ISO_9},
    {"turkish",         IDX_ISO_9}, /* ? */
    {"latin6",          IDX_ISO_10},
    {"nordic",          IDX_ISO_10}, /* ? */
    {"thai",            IDX_ISO_11}, /* ? */
    {"latin7",          IDX_ISO_13},
    {"latin8",          IDX_ISO_14},
    {"latin9",          IDX_ISO_15},
    {"utf8",            IDX_UTF8},
    {"unicode",         IDX_UCS2},
    {"ucs2",            IDX_UCS2},
    {"ucs2be",          IDX_UCS2},
    {"ucs-2be",         IDX_UCS2},
    {"ucs2le",          IDX_UCS2LE},
    {"utf16",           IDX_UTF16},
    {"utf16be",         IDX_UTF16},
    {"utf-16be",        IDX_UTF16},
    {"utf16le",         IDX_UTF16LE},
    {"ucs4",            IDX_UCS4},
    {"ucs4be",          IDX_UCS4},
    {"ucs-4be",         IDX_UCS4},
    {"ucs4le",          IDX_UCS4LE},
    {"utf32",           IDX_UCS4},
    {"utf-32",          IDX_UCS4},
    {"utf32be",         IDX_UCS4},
    {"utf-32be",        IDX_UCS4},
    {"utf32le",         IDX_UCS4LE},
    {"utf-32le",        IDX_UCS4LE},
    {"932",             IDX_CP932},
    {"949",             IDX_CP949},
    {"936",             IDX_CP936},
    {"gbk",             IDX_CP936},
    {"950",             IDX_CP950},
    {"eucjp",           IDX_EUC_JP},
    {"unix-jis",        IDX_EUC_JP},
    {"ujis",            IDX_EUC_JP},
    {"shift-jis",       IDX_SJIS},
    {"euckr",           IDX_EUC_KR},
    {"5601",            IDX_EUC_KR},        /* Sun: KS C 5601 */
    {"euccn",           IDX_EUC_CN},
    {"gb2312",          IDX_EUC_CN},
    {"euctw",           IDX_EUC_TW},
#if defined(WIN3264) || defined(WIN32UNIX) || defined(MACOS_X)
    {"japan",           IDX_CP932},
    {"korea",           IDX_CP949},
    {"prc",             IDX_CP936},
    {"chinese",         IDX_CP936},
    {"taiwan",          IDX_CP950},
    {"big5",            IDX_CP950},
#else
    {"japan",           IDX_EUC_JP},
    {"korea",           IDX_EUC_KR},
    {"prc",             IDX_EUC_CN},
    {"chinese",         IDX_EUC_CN},
    {"taiwan",          IDX_EUC_TW},
    {"cp950",           IDX_BIG5},
    {"950",             IDX_BIG5},
#endif
    {"mac",             IDX_MACROMAN},
    {"mac-roman",       IDX_MACROMAN},
    {NULL,              0}
};

/*
 * Lookup table to quickly get the length in bytes of a UTF-8 character from
 * the first byte of a UTF-8 string.
 * Bytes which are illegal when used as the first byte have a 1.
 * The NUL byte has length 1.
 */
static char utf8len_tab[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1,
};

/*
 * Like utf8len_tab above, but using a zero for illegal lead bytes.
 */
static char utf8len_tab_zero[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0,
};


#ifndef CP_UTF8
# define CP_UTF8 65001        /* magic number from winnls.h */
#endif

#ifdef MACOS_X
#include <CoreServices/CoreServices.h>

static unsigned char *mac_utf16_to_utf8(UniChar *from, size_t fromLen, size_t *actualLen);
static UniChar *mac_utf8_to_utf16(unsigned char *from, size_t fromLen, size_t *actualLen);

/* Converter for composing decomposed HFS+ file paths */
static TECObjectRef gPathConverter;
/* Converter used by mac_utf16_to_utf8 */
static TECObjectRef gUTF16ToUTF8Converter;

/*
 * A Mac version of string_convert_ext() for special cases.
 */
static unsigned char *mac_string_convert(unsigned char *ptr, int len, int *lenp, int fail_on_error, int from_enc, int to_enc, int *unconvlenp)
{
    unsigned char *retval, *d;
    CFStringRef cfstr;
    int buflen, in, out, l, i;
    CFStringEncoding from;
    CFStringEncoding to;

    switch (from_enc) {
        case 'l':   from = kCFStringEncodingISOLatin1; break;
        case 'm':   from = kCFStringEncodingMacRoman; break;
        case 'u':   from = kCFStringEncodingUTF8; break;
        default:    return NULL;
    }
    switch (to_enc) {
        case 'l':   to = kCFStringEncodingISOLatin1; break;
        case 'm':   to = kCFStringEncodingMacRoman; break;
        case 'u':   to = kCFStringEncodingUTF8; break;
        default:    return NULL;
    }

    if (unconvlenp != NULL)
        *unconvlenp = 0;
    cfstr = CFStringCreateWithBytes(NULL, ptr, len, from, 0);

    if(cfstr == NULL)
        fprintf(stderr, "Encoding failed\n");
    /* When conversion failed, try excluding bytes from the end, helps when
     * there is an incomplete byte sequence.  Only do up to 6 bytes to avoid
     * looping a long time when there really is something unconvertible. */
    while (cfstr == NULL && unconvlenp != NULL && len > 1 && *unconvlenp < 6) {
        --len;
        ++*unconvlenp;
        cfstr = CFStringCreateWithBytes(NULL, ptr, len, from, 0);
    }
    if (cfstr == NULL)
        return NULL;

    if (to == kCFStringEncodingUTF8)
        buflen = len * 6 + 1;
    else
        buflen = len + 1;
    retval = malloc(buflen);
    if (retval == NULL)
    {
        CFRelease(cfstr);
        return NULL;
    }

    if (!CFStringGetCString(cfstr, (char *)retval, buflen, to)) {
        CFRelease(cfstr);
        if (fail_on_error)
        {
            free(retval);
            return NULL;
        }

        fprintf(stderr, "Trying char-by-char conversion...\n");
        /* conversion failed for the whole string, but maybe it will work
         * for each character */
        for (d = retval, in = 0, out = 0; in < len && out < buflen - 1;) {
            if (from == kCFStringEncodingUTF8)
                l = utf_ptr2len(ptr + in);
            else
                l = 1;
            cfstr = CFStringCreateWithBytes(NULL, ptr + in, l, from, 0);
            if (cfstr == NULL) {
                *d++ = '?';
                out++;
            } else {
                if (!CFStringGetCString(cfstr, (char *)d, buflen - out, to))
                {
                    *d++ = '?';
                    out++;
                }
                else
                {
                    i = strlen(d);
                    d += i;
                    out += i;
                }
                CFRelease(cfstr);
            }
            in += l;
        }
        *d = '\0';
        if (lenp != NULL)
            *lenp = out;
        return retval;
    }
    CFRelease(cfstr);
    if (lenp != NULL)
        *lenp = strlen(retval);

    return retval;
}

/*
 * Conversion from Apple MacRoman char encoding to UTF-8 or latin1, using
 * standard Carbon framework.
 * Input: "ptr[*sizep]".
 * "real_size" is the size of the buffer that "ptr" points to.
 * output is in-place, "sizep" is adjusted.
 * Returns OK or -1.
 */
static int macroman2enc(unsigned char *ptr, long *sizep, long real_size)
{
    CFStringRef cfstr;
    CFRange r;
    CFIndex len = *sizep;

    /* MacRoman is an 8-bit encoding, no need to move bytes to
     * conv_rest[]. */
    cfstr = CFStringCreateWithBytes(NULL, ptr, len, kCFStringEncodingMacRoman, 0);
    /*
     * If there is a conversion error, try using another
     * conversion.
     */
    if (cfstr == NULL)
        return -1;

    r.location = 0;
    r.length = CFStringGetLength(cfstr);
    if (r.length != CFStringGetBytes(cfstr, r,
            kCFStringEncodingUTF8, 0, /* no lossy conversion */
            0, /* not external representation */
            ptr + *sizep, real_size - *sizep, &len)) {
        CFRelease(cfstr);
        return -1;
    }
    CFRelease(cfstr);
    memmove(ptr, ptr + *sizep, len);
    *sizep = len;

    return 0;
}

/*
 * Conversion from UTF-8 or latin1 to MacRoman.
 * Input: "from[fromlen]"
 * Output: "to[maxtolen]" length in "*tolenp"
 * Unconverted rest in rest[*restlenp].
 * Returns OK or -1.
 */
static int enc2macroman(unsigned char *from, size_t fromlen, unsigned char *to, int  *tolenp, int maxtolen, unsigned char *rest, int *restlenp)
{
    CFStringRef        cfstr;
    CFRange        r;
    CFIndex        l;

    *restlenp = 0;
    cfstr = CFStringCreateWithBytes(NULL, from, fromlen, kCFStringEncodingUTF8, 0);
    while (cfstr == NULL && *restlenp < 3 && fromlen > 1)
    {
        rest[*restlenp++] = from[--fromlen];
        cfstr = CFStringCreateWithBytes(NULL, from, fromlen, kCFStringEncodingUTF8, 0);
    }
    if (cfstr == NULL)
        return -1;

    r.location = 0;
    r.length = CFStringGetLength(cfstr);
    if (r.length != CFStringGetBytes(cfstr, r,
                kCFStringEncodingMacRoman,
                0, /* no lossy conversion */
                0, /* not external representation (since vim
                    * handles this internally */
                to, maxtolen, &l))
    {
        CFRelease(cfstr);
        return -1;
    }
    CFRelease(cfstr);
    *tolenp = l;
    return 0;
}

/*
 * Initializes text converters
 */
static void mac_conv_init()
{
    TextEncoding    utf8_encoding;
    TextEncoding    utf8_hfsplus_encoding;
    TextEncoding    utf8_canon_encoding;
    TextEncoding    utf16_encoding;

    utf8_encoding = CreateTextEncoding(kTextEncodingUnicodeDefault,
            kTextEncodingDefaultVariant, kUnicodeUTF8Format);
    utf8_hfsplus_encoding = CreateTextEncoding(kTextEncodingUnicodeDefault,
            kUnicodeHFSPlusCompVariant, kUnicodeUTF8Format);
    utf8_canon_encoding = CreateTextEncoding(kTextEncodingUnicodeDefault,
            kUnicodeCanonicalCompVariant, kUnicodeUTF8Format);
    utf16_encoding = CreateTextEncoding(kTextEncodingUnicodeDefault,
            kTextEncodingDefaultVariant, kUnicode16BitFormat);

    if (TECCreateConverter(&gPathConverter, utf8_encoding,
                utf8_hfsplus_encoding) != noErr)
        gPathConverter = NULL;

    if (TECCreateConverter(&gUTF16ToUTF8Converter, utf16_encoding,
                utf8_canon_encoding) != noErr)
    {
        /* On pre-10.3, Unicode normalization is not available so
         * fall back to non-normalizing converter */
        if (TECCreateConverter(&gUTF16ToUTF8Converter, utf16_encoding,
                    utf8_encoding) != noErr)
            gUTF16ToUTF8Converter = NULL;
    }
}

/*
 * Destroys text converters
 */
static void mac_conv_cleanup()
{
    if (gUTF16ToUTF8Converter)
    {
        TECDisposeConverter(gUTF16ToUTF8Converter);
        gUTF16ToUTF8Converter = NULL;
    }

    if (gPathConverter)
    {
        TECDisposeConverter(gPathConverter);
        gPathConverter = NULL;
    }
}

/*
 * Converts from UTF-16 UniChars to precomposed UTF-8
 */
static unsigned char *mac_utf16_to_utf8(UniChar *from, size_t fromLen, size_t *actualLen)
{
    ByteCount utf8_len;
    ByteCount inputRead;
    unsigned char *result;

    if (gUTF16ToUTF8Converter) {
        result = malloc(fromLen * 6 + 1);
        if (result && TECConvertText(gUTF16ToUTF8Converter, (ConstTextPtr)from,
                    fromLen, &inputRead, result,
                    (fromLen*6+1)*sizeof(unsigned char), &utf8_len) == noErr) {
            TECFlushText(gUTF16ToUTF8Converter, result, (fromLen*6+1)*sizeof(unsigned char), &inputRead);
            utf8_len += inputRead;
        } else {
            free(result);
            result = NULL;
        }
    } else {
        result = NULL;
    }

    if (actualLen)
        *actualLen = result ? utf8_len : 0;

    return result;
}

/*
 * Converts from UTF-8 to UTF-16 UniChars
 */
static UniChar *mac_utf8_to_utf16(from, fromLen, actualLen)
    unsigned char *from;
    size_t fromLen;
    size_t *actualLen;
{
    CFStringRef  utf8_str;
    CFRange      convertRange;
    UniChar      *result = NULL;

    utf8_str = CFStringCreateWithBytes(NULL, from, fromLen,
            kCFStringEncodingUTF8, FALSE);

    if (utf8_str == NULL) {
        if (actualLen)
            *actualLen = 0;
        return NULL;
    }

    convertRange = CFRangeMake(0, CFStringGetLength(utf8_str));
    result = (UniChar *)malloc(convertRange.length * sizeof(UniChar));

    CFStringGetCharacters(utf8_str, convertRange, result);

    CFRelease(utf8_str);

    if (actualLen)
        *actualLen = convertRange.length * sizeof(UniChar);

    return result;
}
#endif

#ifdef USE_ICONV
/*
 * Convert the string "str[slen]" with iconv().
 * If "unconvlenp" is not NULL handle the string ending in an incomplete
 * sequence and set "*unconvlenp" to the length of it.
 * Returns the converted string in allocated memory.  NULL for an error.
 * If resultlenp is not NULL, sets it to the result length in bytes.
 */
static char *iconv_string(vimconv_T *vcp, char*str, int slen, int *unconvlenp, int *resultlenp)
{
    const char *from;
    size_t fromlen;
    char *to;
    size_t tolen;
    size_t len = 0;
    size_t done = 0;
    char *result = NULL;
    char *p;
    int l;

    from = (char *)str;
    fromlen = slen;
    for (;;) {
        if (len == 0 || errno == E2BIG) {
            /* Allocate enough room for most conversions.  When re-allocating
             * increase the buffer size. */
            len = len + fromlen * 2 + 40;
            p = malloc(len);
            if (p != NULL && done > 0)
                memmove(p, result, done);
            free(result);
            result = p;
            if (result == NULL)        /* out of memory */
                break;
        }

        to = (char *)result + done;
        tolen = len - done - 2;
        /* Avoid a warning for systems with a wrong iconv() prototype by
         * casting the second argument to void *. */
        if (iconv(vcp->vc_fd, (void *)&from, &fromlen, &to, &tolen) != (size_t)-1) {
            /* Finished, append a NUL. */
            *to = '\0';
            break;
        }

        /* Check both ICONV_EINVAL and EINVAL, because the dynamically loaded
         * iconv library may use one of them. */
        if (!vcp->vc_fail && unconvlenp != NULL && errno == EINVAL) {
            /* Handle an incomplete sequence at the end. */
            *to = '\0';
            *unconvlenp = (int)fromlen;
            break;
        }
        /* Check both ICONV_EILSEQ and EILSEQ, because the dynamically loaded
         * iconv library may use one of them. */
        else if (!vcp->vc_fail && (errno == EILSEQ || errno == EINVAL)) {
            /* Can't convert: insert a '?' and skip a character.  This assumes
             * conversion from 'encoding' to something else.  In other
             * situations we don't know what to skip anyway. */
            *to++ = '?';
            from += 1;
            fromlen -= 1;
        } else if (errno != E2BIG) {
            /* conversion failed */
            free(result);
            result = NULL;
            break;
        }
        /* Not enough room or skipping illegal sequence. */
        done = to - (char *)result;
    }

    if (resultlenp != NULL)
        *resultlenp = (int)(to - (char *)result);
    return result;
}
#endif

/*
 * Find encoding "name" in the list of canonical encoding names.
 * Returns -1 if not found.
 */
static int enc_canon_search(const char *name)
{
    int i;
    for (i = 0; i < IDX_COUNT; ++i) {
        if (strcmp(name, enc_canon_table[i].name) == 0)
            return i;
    }
    return -1;
}

/*
 * Find canonical encoding "name" in the list and return its properties.
 * Returns 0 if not found.
 */
int enc_canon_props(const char *name)
{
    int                i;

    i = enc_canon_search(name);
    if (i >= 0)
        return enc_canon_table[i].prop;
#ifdef WIN3264
    if (name[0] == 'c' && name[1] == 'p' && VIM_ISDIGIT(name[2])) {
        CPINFO        cpinfo;

        /* Get info on this codepage to find out what it is. */
        if (GetCPInfo(atoi(name + 2), &cpinfo) != 0)
        {
            if (cpinfo.MaxCharSize == 1) /* some single-byte encoding */
                return ENC_8BIT;
            if (cpinfo.MaxCharSize == 2 &&
                    (cpinfo.LeadByte[0] != 0 || cpinfo.LeadByte[1] != 0))
                /* must be a DBCS encoding */
                return ENC_DBCS;
        }
        return 0;
    }
#endif
    if (strncmp(name, "2byte-", 6) == 0)
        return ENC_DBCS;
    if (strncmp(name, "8bit-", 5) == 0 || strncmp(name, "iso-8859-", 9) == 0)
        return ENC_8BIT;
    return 0;
}

/*
 * As convert_setup(), but only when from_unicode_is_utf8 is TRUE will all
 * "from" unicode charsets be considered utf-8.  Same for "to".
 */
int convert_setup_ext(vimconv_T *vcp, const char *from, int from_unicode_is_utf8, const char *to, int to_unicode_is_utf8)
{
    int from_prop;
    int to_prop;
    int from_is_utf8;
    int to_is_utf8;
    /* Reset to no conversion. */
#ifdef USE_ICONV
    if (vcp->vc_type == CONV_ICONV && vcp->vc_fd != (iconv_t)-1)
        iconv_close(vcp->vc_fd);
#endif
    vcp->vc_type = CONV_NONE;
    vcp->vc_factor = 1;
    vcp->vc_fail = 0;

    /* No conversion when one of the names is empty or they are equal. */
    if (from == NULL || *from == '\0' || to == NULL || *to == '\0'
            || strcmp(from, to) == 0)
        return 0;

    from_prop = enc_canon_props(from);
    to_prop = enc_canon_props(to);
    if (from_unicode_is_utf8)
        from_is_utf8 = from_prop & ENC_UNICODE;
    else
        from_is_utf8 = from_prop == ENC_UNICODE;
    if (to_unicode_is_utf8)
        to_is_utf8 = to_prop & ENC_UNICODE;
    else
        to_is_utf8 = to_prop == ENC_UNICODE;

    if ((from_prop & ENC_LATIN1) && to_is_utf8) {
        /* Internal latin1 -> utf-8 conversion. */
        vcp->vc_type = CONV_TO_UTF8;
        vcp->vc_factor = 2;        /* up to twice as long */
    } else if ((from_prop & ENC_LATIN9) && to_is_utf8) {
        /* Internal latin9 -> utf-8 conversion. */
        vcp->vc_type = CONV_9_TO_UTF8;
        vcp->vc_factor = 3;        /* up to three as long (euro sign) */
    } else if (from_is_utf8 && (to_prop & ENC_LATIN1)) {
        /* Internal utf-8 -> latin1 conversion. */
        vcp->vc_type = CONV_TO_LATIN1;
    } else if (from_is_utf8 && (to_prop & ENC_LATIN9)) {
        /* Internal utf-8 -> latin9 conversion. */
        vcp->vc_type = CONV_TO_LATIN9;
    }
#ifdef WIN3264
    /* Win32-specific codepage <-> codepage conversion without iconv. */
    else if ((from_is_utf8 || encname2codepage(from) > 0)
            && (to_is_utf8 || encname2codepage(to) > 0))
    {
        vcp->vc_type = CONV_CODEPAGE;
        vcp->vc_factor = 2;        /* up to twice as long */
        vcp->vc_cpfrom = from_is_utf8 ? 0 : encname2codepage(from);
        vcp->vc_cpto = to_is_utf8 ? 0 : encname2codepage(to);
    }
#endif
#ifdef MACOS_X
    else if ((from_prop & ENC_MACROMAN) && (to_prop & ENC_LATIN1)) {
        vcp->vc_type = CONV_MAC_LATIN1;
    } else if ((from_prop & ENC_MACROMAN) && to_is_utf8) {
        vcp->vc_type = CONV_MAC_UTF8;
        vcp->vc_factor = 2;        /* up to twice as long */
    } else if ((from_prop & ENC_LATIN1) && (to_prop & ENC_MACROMAN)) {
        vcp->vc_type = CONV_LATIN1_MAC;
    } else if (from_is_utf8 && (to_prop & ENC_MACROMAN)) {
        vcp->vc_type = CONV_UTF8_MAC;
    }
#endif
#ifdef USE_ICONV
    else {
        /* Use iconv() for conversion. */
        vcp->vc_fd = (iconv_t)iconv_open(
                to_is_utf8 ? (char *)"utf-8" : to,
                from_is_utf8 ? (char *)"utf-8" : from);
        if (vcp->vc_fd != (iconv_t)-1) {
            vcp->vc_type = CONV_ICONV;
            vcp->vc_factor = 4;        /* could be longer too... */
        }
    }
#endif
    if (vcp->vc_type == CONV_NONE)
        return -1;

    return 0;
}

/*
 * Setup "vcp" for conversion from "from" to "to".
 * The names must have been made canonical with enc_canonize().
 * vcp->vc_type must have been initialized to CONV_NONE.
 * Note: cannot be used for conversion from/to ucs-2 and ucs-4 (will use utf-8
 * instead).
 * Afterwards invoke with "from" and "to" equal to NULL to cleanup.
 * Return -1 when conversion is not supported, 0 otherwise.
 */
int convert_setup(vimconv_T *vcp, const char *from, const char *to)
{
    return convert_setup_ext(vcp, from, 1, to, 1);
}

/*
 * Convert Unicode character "c" to UTF-8 string in "buf[]".
 * Returns the number of bytes.
 * This does not include composing characters.
 */
static int utf_char2bytes(int c, char *buf)
{
    if (c < 0x80) /* 7 bits */
    {
        buf[0] = c;
        return 1;
    }
    if (c < 0x800) /* 11 bits */
    {
        buf[0] = 0xc0 + ((unsigned int)c >> 6);
        buf[1] = 0x80 + (c & 0x3f);
        return 2;
    }
    if (c < 0x10000) /* 16 bits */
    {
        buf[0] = 0xe0 + ((unsigned int)c >> 12);
        buf[1] = 0x80 + (((unsigned int)c >> 6) & 0x3f);
        buf[2] = 0x80 + (c & 0x3f);
        return 3;
    }
    if (c < 0x200000) /* 21 bits */
    {
        buf[0] = 0xf0 + ((unsigned int)c >> 18);
        buf[1] = 0x80 + (((unsigned int)c >> 12) & 0x3f);
        buf[2] = 0x80 + (((unsigned int)c >> 6) & 0x3f);
        buf[3] = 0x80 + (c & 0x3f);
        return 4;
    }
    if (c < 0x4000000) /* 26 bits */
    {
        buf[0] = 0xf8 + ((unsigned int)c >> 24);
        buf[1] = 0x80 + (((unsigned int)c >> 18) & 0x3f);
        buf[2] = 0x80 + (((unsigned int)c >> 12) & 0x3f);
        buf[3] = 0x80 + (((unsigned int)c >> 6) & 0x3f);
        buf[4] = 0x80 + (c & 0x3f);
        return 5;
    }
    /* 31 bits */
    buf[0] = 0xfc + ((unsigned int)c >> 30);
    buf[1] = 0x80 + (((unsigned int)c >> 24) & 0x3f);
    buf[2] = 0x80 + (((unsigned int)c >> 18) & 0x3f);
    buf[3] = 0x80 + (((unsigned int)c >> 12) & 0x3f);
    buf[4] = 0x80 + (((unsigned int)c >> 6) & 0x3f);
    buf[5] = 0x80 + (c & 0x3f);
    return 6;
}
    
struct interval {
    long first;
    long last;
};

/*
 * Return TRUE if "c" is in "table[size / sizeof(struct interval)]".
 */
static int intable(const struct interval *table, size_t size, int c)
{
    int mid, bot, top;

    /* first quick check for Latin1 etc. characters */
    if (c < table[0].first)
        return 0;

    /* binary search in table */
    bot = 0;
    top = (int)(size / sizeof(struct interval) - 1);
    while (top >= bot) {
        mid = (bot + top) / 2;
        if (table[mid].last < c)
            bot = mid + 1;
        else if (table[mid].first > c)
            top = mid - 1;
        else
            return 1;
    }
    return 0;
}

/*
 * Return TRUE for characters that can be displayed in a normal way.
 * Only for characters of 0x100 and above!
 */
int utf_printable(int c)
{
#ifdef USE_WCHAR_FUNCTIONS
    /*
     * Assume the iswprint() library function works better than our own stuff.
     */
    return iswprint(c);
#else
    /* Sorted list of non-overlapping intervals.
     * 0xd800-0xdfff is reserved for UTF-16, actually illegal. */
    static struct interval nonprint[] = {
        {0x070f, 0x070f},
        {0x180b, 0x180e},
        {0x200b, 0x200f},
        {0x202a, 0x202e},
        {0x206a, 0x206f},
        {0xd800, 0xdfff},
        {0xfeff, 0xfeff},
        {0xfff9, 0xfffb},
        {0xfffe, 0xffff}
    };

    return !intable(nonprint, sizeof(nonprint), c);
#endif
}

/*
 * For UTF-8 character "c" return 2 for a double-width character, 1 for others.
 * Returns 4 or 6 for an unprintable character.
 * Is only correct for characters >= 0x80.
 * When p_ambw is "double", return 2 for a character with East Asian Width
 * class 'A'(mbiguous).
 */
static int utf_char2cells(int c)
{
    /* Sorted list of non-overlapping intervals of East Asian double width
     * characters, generated with ../runtime/tools/unicode.vim. */
    static const struct interval doublewidth[] = {
        {0x1100, 0x115f},
        {0x11a3, 0x11a7},
        {0x11fa, 0x11ff},
        {0x2329, 0x232a},
        {0x2e80, 0x2e99},
        {0x2e9b, 0x2ef3},
        {0x2f00, 0x2fd5},
        {0x2ff0, 0x2ffb},
        {0x3000, 0x3029},
        {0x3030, 0x303e},
        {0x3041, 0x3096},
        {0x309b, 0x30ff},
        {0x3105, 0x312d},
        {0x3131, 0x318e},
        {0x3190, 0x31b7},
        {0x31c0, 0x31e3},
        {0x31f0, 0x321e},
        {0x3220, 0x3247},
        {0x3250, 0x32fe},
        {0x3300, 0x4dbf},
        {0x4e00, 0xa48c},
        {0xa490, 0xa4c6},
        {0xa960, 0xa97c},
        {0xac00, 0xd7a3},
        {0xd7b0, 0xd7c6},
        {0xd7cb, 0xd7fb},
        {0xf900, 0xfaff},
        {0xfe10, 0xfe19},
        {0xfe30, 0xfe52},
        {0xfe54, 0xfe66},
        {0xfe68, 0xfe6b},
        {0xff01, 0xff60},
        {0xffe0, 0xffe6},
        {0x1f200, 0x1f200},
        {0x1f210, 0x1f231},
        {0x1f240, 0x1f248},
        {0x20000, 0x2fffd},
        {0x30000, 0x3fffd}
    };
    /* Sorted list of non-overlapping intervals of East Asian Ambiguous
     * characters, generated with ../runtime/tools/unicode.vim. */
    static const struct interval ambiguous[] = {
        {0x00a1, 0x00a1},
        {0x00a4, 0x00a4},
        {0x00a7, 0x00a8},
        {0x00aa, 0x00aa},
        {0x00ad, 0x00ae},
        {0x00b0, 0x00b4},
        {0x00b6, 0x00ba},
        {0x00bc, 0x00bf},
        {0x00c6, 0x00c6},
        {0x00d0, 0x00d0},
        {0x00d7, 0x00d8},
        {0x00de, 0x00e1},
        {0x00e6, 0x00e6},
        {0x00e8, 0x00ea},
        {0x00ec, 0x00ed},
        {0x00f0, 0x00f0},
        {0x00f2, 0x00f3},
        {0x00f7, 0x00fa},
        {0x00fc, 0x00fc},
        {0x00fe, 0x00fe},
        {0x0101, 0x0101},
        {0x0111, 0x0111},
        {0x0113, 0x0113},
        {0x011b, 0x011b},
        {0x0126, 0x0127},
        {0x012b, 0x012b},
        {0x0131, 0x0133},
        {0x0138, 0x0138},
        {0x013f, 0x0142},
        {0x0144, 0x0144},
        {0x0148, 0x014b},
        {0x014d, 0x014d},
        {0x0152, 0x0153},
        {0x0166, 0x0167},
        {0x016b, 0x016b},
        {0x01ce, 0x01ce},
        {0x01d0, 0x01d0},
        {0x01d2, 0x01d2},
        {0x01d4, 0x01d4},
        {0x01d6, 0x01d6},
        {0x01d8, 0x01d8},
        {0x01da, 0x01da},
        {0x01dc, 0x01dc},
        {0x0251, 0x0251},
        {0x0261, 0x0261},
        {0x02c4, 0x02c4},
        {0x02c7, 0x02c7},
        {0x02c9, 0x02cb},
        {0x02cd, 0x02cd},
        {0x02d0, 0x02d0},
        {0x02d8, 0x02db},
        {0x02dd, 0x02dd},
        {0x02df, 0x02df},
        {0x0391, 0x03a1},
        {0x03a3, 0x03a9},
        {0x03b1, 0x03c1},
        {0x03c3, 0x03c9},
        {0x0401, 0x0401},
        {0x0410, 0x044f},
        {0x0451, 0x0451},
        {0x2010, 0x2010},
        {0x2013, 0x2016},
        {0x2018, 0x2019},
        {0x201c, 0x201d},
        {0x2020, 0x2022},
        {0x2024, 0x2027},
        {0x2030, 0x2030},
        {0x2032, 0x2033},
        {0x2035, 0x2035},
        {0x203b, 0x203b},
        {0x203e, 0x203e},
        {0x2074, 0x2074},
        {0x207f, 0x207f},
        {0x2081, 0x2084},
        {0x20ac, 0x20ac},
        {0x2103, 0x2103},
        {0x2105, 0x2105},
        {0x2109, 0x2109},
        {0x2113, 0x2113},
        {0x2116, 0x2116},
        {0x2121, 0x2122},
        {0x2126, 0x2126},
        {0x212b, 0x212b},
        {0x2153, 0x2154},
        {0x215b, 0x215e},
        {0x2160, 0x216b},
        {0x2170, 0x2179},
        {0x2189, 0x2189},
        {0x2190, 0x2199},
        {0x21b8, 0x21b9},
        {0x21d2, 0x21d2},
        {0x21d4, 0x21d4},
        {0x21e7, 0x21e7},
        {0x2200, 0x2200},
        {0x2202, 0x2203},
        {0x2207, 0x2208},
        {0x220b, 0x220b},
        {0x220f, 0x220f},
        {0x2211, 0x2211},
        {0x2215, 0x2215},
        {0x221a, 0x221a},
        {0x221d, 0x2220},
        {0x2223, 0x2223},
        {0x2225, 0x2225},
        {0x2227, 0x222c},
        {0x222e, 0x222e},
        {0x2234, 0x2237},
        {0x223c, 0x223d},
        {0x2248, 0x2248},
        {0x224c, 0x224c},
        {0x2252, 0x2252},
        {0x2260, 0x2261},
        {0x2264, 0x2267},
        {0x226a, 0x226b},
        {0x226e, 0x226f},
        {0x2282, 0x2283},
        {0x2286, 0x2287},
        {0x2295, 0x2295},
        {0x2299, 0x2299},
        {0x22a5, 0x22a5},
        {0x22bf, 0x22bf},
        {0x2312, 0x2312},
        {0x2460, 0x24e9},
        {0x24eb, 0x254b},
        {0x2550, 0x2573},
        {0x2580, 0x258f},
        {0x2592, 0x2595},
        {0x25a0, 0x25a1},
        {0x25a3, 0x25a9},
        {0x25b2, 0x25b3},
        {0x25b6, 0x25b7},
        {0x25bc, 0x25bd},
        {0x25c0, 0x25c1},
        {0x25c6, 0x25c8},
        {0x25cb, 0x25cb},
        {0x25ce, 0x25d1},
        {0x25e2, 0x25e5},
        {0x25ef, 0x25ef},
        {0x2605, 0x2606},
        {0x2609, 0x2609},
        {0x260e, 0x260f},
        {0x2614, 0x2615},
        {0x261c, 0x261c},
        {0x261e, 0x261e},
        {0x2640, 0x2640},
        {0x2642, 0x2642},
        {0x2660, 0x2661},
        {0x2663, 0x2665},
        {0x2667, 0x266a},
        {0x266c, 0x266d},
        {0x266f, 0x266f},
        {0x269e, 0x269f},
        {0x26be, 0x26bf},
        {0x26c4, 0x26cd},
        {0x26cf, 0x26e1},
        {0x26e3, 0x26e3},
        {0x26e8, 0x26ff},
        {0x273d, 0x273d},
        {0x2757, 0x2757},
        {0x2776, 0x277f},
        {0x2b55, 0x2b59},
        {0x3248, 0x324f},
        {0xe000, 0xf8ff},
        {0xfffd, 0xfffd},
        {0x1f100, 0x1f10a},
        {0x1f110, 0x1f12d},
        {0x1f131, 0x1f131},
        {0x1f13d, 0x1f13d},
        {0x1f13f, 0x1f13f},
        {0x1f142, 0x1f142},
        {0x1f146, 0x1f146},
        {0x1f14a, 0x1f14e},
        {0x1f157, 0x1f157},
        {0x1f15f, 0x1f15f},
        {0x1f179, 0x1f179},
        {0x1f17b, 0x1f17c},
        {0x1f17f, 0x1f17f},
        {0x1f18a, 0x1f18d},
        {0x1f190, 0x1f190},
        {0xf0000, 0xffffd},
        {0x100000, 0x10fffd}
    };

    if (c >= 0x100) {
#ifdef USE_WCHAR_FUNCTIONS
        /*
         * Assume the library function wcwidth() works better than our own
         * stuff.  It should return 1 for ambiguous width chars!
         */
        int n = wcwidth(c);

        if (n < 0)
            return 6;                /* unprintable, displays <xxxx> */
        if (n > 1)
            return n;
#else
        if (!utf_printable(c))
            return 6;                /* unprintable, displays <xxxx> */
        if (intable(doublewidth, sizeof(doublewidth), c))
            return 2;
#endif
    }

    /* Characters below 0x100 are influenced by 'isprint' option */
    else if (c >= 0x80 && !isprint(c))
        return 4;                /* unprintable, displays <xx> */

    if (c >= 0x80 /* && *p_ambw == 'd' */ && intable(ambiguous, sizeof(ambiguous), c))
        return 2;

    return 1;
}

/*
 * Return TRUE if "c" is a composing UTF-8 character.  This means it will be
 * drawn on top of the preceding character.
 * Based on code from Markus Kuhn.
 */
int utf_iscomposing(int	c)
{
    /* Sorted list of non-overlapping intervals.
     * Generated by ../runtime/tools/unicode.vim. */
    static struct interval combining[] = {
        {0x0300, 0x036f},
        {0x0483, 0x0489},
        {0x0591, 0x05bd},
        {0x05bf, 0x05bf},
        {0x05c1, 0x05c2},
        {0x05c4, 0x05c5},
        {0x05c7, 0x05c7},
        {0x0610, 0x061a},
        {0x064b, 0x065e},
        {0x0670, 0x0670},
        {0x06d6, 0x06dc},
        {0x06de, 0x06e4},
        {0x06e7, 0x06e8},
        {0x06ea, 0x06ed},
        {0x0711, 0x0711},
        {0x0730, 0x074a},
        {0x07a6, 0x07b0},
        {0x07eb, 0x07f3},
        {0x0816, 0x0819},
        {0x081b, 0x0823},
        {0x0825, 0x0827},
        {0x0829, 0x082d},
        {0x0900, 0x0903},
        {0x093c, 0x093c},
        {0x093e, 0x094e},
        {0x0951, 0x0955},
        {0x0962, 0x0963},
        {0x0981, 0x0983},
        {0x09bc, 0x09bc},
        {0x09be, 0x09c4},
        {0x09c7, 0x09c8},
        {0x09cb, 0x09cd},
        {0x09d7, 0x09d7},
        {0x09e2, 0x09e3},
        {0x0a01, 0x0a03},
        {0x0a3c, 0x0a3c},
        {0x0a3e, 0x0a42},
        {0x0a47, 0x0a48},
        {0x0a4b, 0x0a4d},
        {0x0a51, 0x0a51},
        {0x0a70, 0x0a71},
        {0x0a75, 0x0a75},
        {0x0a81, 0x0a83},
        {0x0abc, 0x0abc},
        {0x0abe, 0x0ac5},
        {0x0ac7, 0x0ac9},
        {0x0acb, 0x0acd},
        {0x0ae2, 0x0ae3},
        {0x0b01, 0x0b03},
        {0x0b3c, 0x0b3c},
        {0x0b3e, 0x0b44},
        {0x0b47, 0x0b48},
        {0x0b4b, 0x0b4d},
        {0x0b56, 0x0b57},
        {0x0b62, 0x0b63},
        {0x0b82, 0x0b82},
        {0x0bbe, 0x0bc2},
        {0x0bc6, 0x0bc8},
        {0x0bca, 0x0bcd},
        {0x0bd7, 0x0bd7},
        {0x0c01, 0x0c03},
        {0x0c3e, 0x0c44},
        {0x0c46, 0x0c48},
        {0x0c4a, 0x0c4d},
        {0x0c55, 0x0c56},
        {0x0c62, 0x0c63},
        {0x0c82, 0x0c83},
        {0x0cbc, 0x0cbc},
        {0x0cbe, 0x0cc4},
        {0x0cc6, 0x0cc8},
        {0x0cca, 0x0ccd},
        {0x0cd5, 0x0cd6},
        {0x0ce2, 0x0ce3},
        {0x0d02, 0x0d03},
        {0x0d3e, 0x0d44},
        {0x0d46, 0x0d48},
        {0x0d4a, 0x0d4d},
        {0x0d57, 0x0d57},
        {0x0d62, 0x0d63},
        {0x0d82, 0x0d83},
        {0x0dca, 0x0dca},
        {0x0dcf, 0x0dd4},
        {0x0dd6, 0x0dd6},
        {0x0dd8, 0x0ddf},
        {0x0df2, 0x0df3},
        {0x0e31, 0x0e31},
        {0x0e34, 0x0e3a},
        {0x0e47, 0x0e4e},
        {0x0eb1, 0x0eb1},
        {0x0eb4, 0x0eb9},
        {0x0ebb, 0x0ebc},
        {0x0ec8, 0x0ecd},
        {0x0f18, 0x0f19},
        {0x0f35, 0x0f35},
        {0x0f37, 0x0f37},
        {0x0f39, 0x0f39},
        {0x0f3e, 0x0f3f},
        {0x0f71, 0x0f84},
        {0x0f86, 0x0f87},
        {0x0f90, 0x0f97},
        {0x0f99, 0x0fbc},
        {0x0fc6, 0x0fc6},
        {0x102b, 0x103e},
        {0x1056, 0x1059},
        {0x105e, 0x1060},
        {0x1062, 0x1064},
        {0x1067, 0x106d},
        {0x1071, 0x1074},
        {0x1082, 0x108d},
        {0x108f, 0x108f},
        {0x109a, 0x109d},
        {0x135f, 0x135f},
        {0x1712, 0x1714},
        {0x1732, 0x1734},
        {0x1752, 0x1753},
        {0x1772, 0x1773},
        {0x17b6, 0x17d3},
        {0x17dd, 0x17dd},
        {0x180b, 0x180d},
        {0x18a9, 0x18a9},
        {0x1920, 0x192b},
        {0x1930, 0x193b},
        {0x19b0, 0x19c0},
        {0x19c8, 0x19c9},
        {0x1a17, 0x1a1b},
        {0x1a55, 0x1a5e},
        {0x1a60, 0x1a7c},
        {0x1a7f, 0x1a7f},
        {0x1b00, 0x1b04},
        {0x1b34, 0x1b44},
        {0x1b6b, 0x1b73},
        {0x1b80, 0x1b82},
        {0x1ba1, 0x1baa},
        {0x1c24, 0x1c37},
        {0x1cd0, 0x1cd2},
        {0x1cd4, 0x1ce8},
        {0x1ced, 0x1ced},
        {0x1cf2, 0x1cf2},
        {0x1dc0, 0x1de6},
        {0x1dfd, 0x1dff},
        {0x20d0, 0x20f0},
        {0x2cef, 0x2cf1},
        {0x2de0, 0x2dff},
        {0x302a, 0x302f},
        {0x3099, 0x309a},
        {0xa66f, 0xa672},
        {0xa67c, 0xa67d},
        {0xa6f0, 0xa6f1},
        {0xa802, 0xa802},
        {0xa806, 0xa806},
        {0xa80b, 0xa80b},
        {0xa823, 0xa827},
        {0xa880, 0xa881},
        {0xa8b4, 0xa8c4},
        {0xa8e0, 0xa8f1},
        {0xa926, 0xa92d},
        {0xa947, 0xa953},
        {0xa980, 0xa983},
        {0xa9b3, 0xa9c0},
        {0xaa29, 0xaa36},
        {0xaa43, 0xaa43},
        {0xaa4c, 0xaa4d},
        {0xaa7b, 0xaa7b},
        {0xaab0, 0xaab0},
        {0xaab2, 0xaab4},
        {0xaab7, 0xaab8},
        {0xaabe, 0xaabf},
        {0xaac1, 0xaac1},
        {0xabe3, 0xabea},
        {0xabec, 0xabed},
        {0xfb1e, 0xfb1e},
        {0xfe00, 0xfe0f},
        {0xfe20, 0xfe26},
        {0x101fd, 0x101fd},
        {0x10a01, 0x10a03},
        {0x10a05, 0x10a06},
        {0x10a0c, 0x10a0f},
        {0x10a38, 0x10a3a},
        {0x10a3f, 0x10a3f},
        {0x11080, 0x11082},
        {0x110b0, 0x110ba},
        {0x1d165, 0x1d169},
        {0x1d16d, 0x1d172},
        {0x1d17b, 0x1d182},
        {0x1d185, 0x1d18b},
        {0x1d1aa, 0x1d1ad},
        {0x1d242, 0x1d244},
        {0xe0100, 0xe01ef}
    };

    return intable(combining, sizeof(combining), c);
}

/*
 * Convert a UTF-8 byte sequence to a wide character.
 * If the sequence is illegal or truncated by a NUL the first byte is
 * returned.
 * Does not include composing characters, of course.
 */
int utf_ptr2char(const char *p)
{
    const unsigned char *_p = (const unsigned char *)p;
    int len;

    if (_p[0] < 0x80)        /* be quick for ASCII */
        return _p[0];

    len = utf8len_tab_zero[_p[0]];
    if (len > 1 && (_p[1] & 0xc0) == 0x80) {
        if (len == 2)
            return ((_p[0] & 0x1f) << 6) + (_p[1] & 0x3f);
        if ((_p[2] & 0xc0) == 0x80) {
            if (len == 3) {
                return ((_p[0] & 0x0f) << 12) + ((_p[1] & 0x3f) << 6)
                    + (_p[2] & 0x3f);
            }
            if ((_p[3] & 0xc0) == 0x80) {
                if (len == 4) {
                    return ((_p[0] & 0x07) << 18) + ((_p[1] & 0x3f) << 12)
                        + ((_p[2] & 0x3f) << 6) + (_p[3] & 0x3f);
                }
                if ((_p[4] & 0xc0) == 0x80) {
                    if (len == 5) {
                        return ((_p[0] & 0x03) << 24) + ((_p[1] & 0x3f) << 18)
                            + ((_p[2] & 0x3f) << 12) + ((_p[3] & 0x3f) << 6)
                            + (_p[4] & 0x3f);
                    }
                    if ((_p[5] & 0xc0) == 0x80 && len == 6) {
                        return ((_p[0] & 0x01) << 30) + ((_p[1] & 0x3f) << 24)
                            + ((_p[2] & 0x3f) << 18) + ((_p[3] & 0x3f) << 12)
                            + ((_p[4] & 0x3f) << 6) + (_p[5] & 0x3f);
                    }
                }
            }
        }
    }
    /* Illegal value, just return the first byte */
    return _p[0];
}

/*
 * Get the length of UTF-8 byte sequence "p[size]".  Does not include any
 * following composing characters.
 * Returns 1 for "".
 * Returns 1 for an illegal byte sequence (also in incomplete byte seq.).
 * Returns number > "size" for an incomplete byte sequence.
 * Never returns zero.
 */
size_t utf_ptr2len_len(const char *p, size_t size)
{
    size_t i, m, len = utf8len_tab[*(const unsigned char *)p];
    if (len == 1)
        return 1;        /* NUL, ascii or illegal lead byte */
    if (len > size)
        m = size;        /* incomplete byte sequence. */
    else
        m = len;
    for (i = 1; i < m; ++i)
        if ((((const unsigned char *)p)[i] & 0xc0) != 0x80)
            return 1;
    return len;
}


/*
 * Like string_convert(), but when "unconvlenp" is not NULL and there are is
 * an incomplete sequence at the end it is not converted and "*unconvlenp" is
 * set to the number of remaining bytes.
 */
char *string_convert_ext(vimconv_T *vcp, char *ptr, int *lenp, int *unconvlenp)
{
    char *retval = NULL;
    char *d;
    int len;
    int i;
    int l;
    int c;

    if (lenp == NULL)
        len = (int)strlen(ptr);
    else
        len = *lenp;
    if (len == 0)
        return strdup((char *)"");

    switch (vcp->vc_type)
    {
        case CONV_TO_UTF8:        /* latin1 to utf-8 conversion */
            retval = malloc(len * 2 + 1);
            if (retval == NULL)
                break;
            d = retval;
            for (i = 0; i < len; ++i)
            {
                c = ptr[i];
                if (c < 0x80)
                    *d++ = c;
                else
                {
                    *d++ = 0xc0 + ((unsigned)c >> 6);
                    *d++ = 0x80 + (c & 0x3f);
                }
            }
            *d = '\0';
            if (lenp != NULL)
                *lenp = (int)(d - retval);
            break;

        case CONV_9_TO_UTF8:        /* latin9 to utf-8 conversion */
            retval = malloc(len * 3 + 1);
            if (retval == NULL)
                break;
            d = retval;
            for (i = 0; i < len; ++i)
            {
                c = ptr[i];
                switch (c)
                {
                    case 0xa4: c = 0x20ac; break;   /* euro */
                    case 0xa6: c = 0x0160; break;   /* S hat */
                    case 0xa8: c = 0x0161; break;   /* S -hat */
                    case 0xb4: c = 0x017d; break;   /* Z hat */
                    case 0xb8: c = 0x017e; break;   /* Z -hat */
                    case 0xbc: c = 0x0152; break;   /* OE */
                    case 0xbd: c = 0x0153; break;   /* oe */
                    case 0xbe: c = 0x0178; break;   /* Y */
                }
                d += utf_char2bytes(c, d);
            }
            *d = '\0';
            if (lenp != NULL)
                *lenp = (int)(d - retval);
            break;

        case CONV_TO_LATIN1:        /* utf-8 to latin1 conversion */
        case CONV_TO_LATIN9:        /* utf-8 to latin9 conversion */
            retval = malloc(len + 1);
            if (retval == NULL)
                break;
            d = retval;
            for (i = 0; i < len; ++i)
            {
                l = utf_ptr2len_len(ptr + i, len - i);
                if (l == 0)
                    *d++ = '\0';
                else if (l == 1)
                {
                    int l_w = utf8len_tab_zero[ptr[i]];

                    if (l_w == 0)
                    {
                        /* Illegal utf-8 byte cannot be converted */
                        free(retval);
                        return NULL;
                    }
                    if (unconvlenp != NULL && l_w > len - i)
                    {
                        /* Incomplete sequence at the end. */
                        *unconvlenp = len - i;
                        break;
                    }
                    *d++ = ptr[i];
                }
                else
                {
                    c = utf_ptr2char(ptr + i);
                    if (vcp->vc_type == CONV_TO_LATIN9)
                        switch (c)
                        {
                            case 0x20ac: c = 0xa4; break;   /* euro */
                            case 0x0160: c = 0xa6; break;   /* S hat */
                            case 0x0161: c = 0xa8; break;   /* S -hat */
                            case 0x017d: c = 0xb4; break;   /* Z hat */
                            case 0x017e: c = 0xb8; break;   /* Z -hat */
                            case 0x0152: c = 0xbc; break;   /* OE */
                            case 0x0153: c = 0xbd; break;   /* oe */
                            case 0x0178: c = 0xbe; break;   /* Y */
                            case 0xa4:
                            case 0xa6:
                            case 0xa8:
                            case 0xb4:
                            case 0xb8:
                            case 0xbc:
                            case 0xbd:
                            case 0xbe: c = 0x100; break; /* not in latin9 */
                        }
                    if (!utf_iscomposing(c))        /* skip composing chars */
                    {
                        if (c < 0x100)
                            *d++ = c;
                        else if (vcp->vc_fail)
                        {
                            free(retval);
                            return NULL;
                        }
                        else
                        {
                            *d++ = 0xbf;
                            if (utf_char2cells(c) > 1)
                                *d++ = '?';
                        }
                    }
                    i += l - 1;
                }
            }
            *d = '\0';
            if (lenp != NULL)
                *lenp = (int)(d - retval);
            break;

#ifdef MACOS_CONVERT
        case CONV_MAC_LATIN1:
            retval = mac_string_convert(ptr, len, lenp, vcp->vc_fail,
                                        'm', 'l', unconvlenp);
            break;

        case CONV_LATIN1_MAC:
            retval = mac_string_convert(ptr, len, lenp, vcp->vc_fail,
                                        'l', 'm', unconvlenp);
            break;

        case CONV_MAC_UTF8:
            retval = mac_string_convert(ptr, len, lenp, vcp->vc_fail,
                                        'm', 'u', unconvlenp);
            break;

        case CONV_UTF8_MAC:
            retval = mac_string_convert(ptr, len, lenp, vcp->vc_fail,
                                        'u', 'm', unconvlenp);
            break;
#endif

#ifdef USE_ICONV
        case CONV_ICONV:        /* conversion with output_conv.vc_fd */
            retval = iconv_string(vcp, ptr, len, unconvlenp, lenp);
            break;
#endif
#ifdef WIN3264
        case CONV_CODEPAGE:                /* codepage -> codepage */
        {
            int retlen;
            int tmp_len;
            unsigned short *tmp;

            /* 1. codepage/UTF-8  ->  ucs-2. */
            if (vcp->vc_cpfrom == 0)
                tmp_len = utf8_to_utf16(ptr, len, NULL, NULL);
            else
                tmp_len = MultiByteToWideChar(vcp->vc_cpfrom, 0,
                                                              ptr, len, 0, 0);
            tmp = (unsigned short *)malloc(sizeof(unsigned short) * tmp_len);
            if (tmp == NULL)
                break;
            if (vcp->vc_cpfrom == 0)
                utf8_to_utf16(ptr, len, tmp, unconvlenp);
            else
                MultiByteToWideChar(vcp->vc_cpfrom, 0, ptr, len, tmp, tmp_len);

            /* 2. ucs-2  ->  codepage/UTF-8. */
            if (vcp->vc_cpto == 0)
                retlen = utf16_to_utf8(tmp, tmp_len, NULL);
            else
                retlen = WideCharToMultiByte(vcp->vc_cpto, 0,
                                                    tmp, tmp_len, 0, 0, 0, 0);
            retval = malloc(retlen + 1);
            if (retval != NULL) {
                if (vcp->vc_cpto == 0)
                    utf16_to_utf8(tmp, tmp_len, retval);
                else
                    WideCharToMultiByte(vcp->vc_cpto, 0,
                                          tmp, tmp_len, retval, retlen, 0, 0);
                retval[retlen] = NUL;
                if (lenp != NULL)
                    *lenp = retlen;
            }
            free(tmp);
            break;
        }
#endif
    }

    return retval;
}

/*
 * Convert text "ptr[*lenp]" according to "vcp".
 * Returns the result in allocated memory and sets "*lenp".
 * When "lenp" is NULL, use NUL terminated strings.
 * Illegal chars are often changed to "?", unless vcp->vc_fail is set.
 * When something goes wrong, NULL is returned and "*lenp" is unchanged.
 */
char *string_convert(vimconv_T *vcp, char *ptr, int *lenp)
{
    return string_convert_ext(vcp, ptr, lenp, NULL);
}

void conv_init()
{
#ifdef MACOS_X
    mac_conv_init();
#endif
}

void conv_cleanup()
{
#ifdef MACOS_X
    mac_conv_cleanup();
#endif
}
