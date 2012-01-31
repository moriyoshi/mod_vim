#ifndef CONV_H
#define CONV_H

#ifdef USE_ICONV
#include <iconv.h>
#endif

/*
 * Used for conversion of terminal I/O and script files.
 */
typedef struct
{
    int		vc_type;	/* zero or one of the CONV_ values */
    int		vc_factor;	/* max. expansion factor */
# ifdef WIN3264
    int		vc_cpfrom;	/* codepage to convert from (CONV_CODEPAGE) */
    int		vc_cpto;	/* codepage to convert to (CONV_CODEPAGE) */
# endif
# ifdef USE_ICONV
    iconv_t	vc_fd;		/* for CONV_ICONV */
# endif
    int		vc_fail;	/* fail for invalid char, don't use '?' */
} vimconv_T;

# define ENC_8BIT	0x01
# define ENC_DBCS	0x02
# define ENC_UNICODE	0x04

# define ENC_ENDIAN_B	0x10	    /* Unicode: Big endian */
# define ENC_ENDIAN_L	0x20	    /* Unicode: Little endian */

# define ENC_2BYTE	0x40	    /* Unicode: UCS-2 */
# define ENC_4BYTE	0x80	    /* Unicode: UCS-4 */
# define ENC_2WORD	0x100	    /* Unicode: UTF-16 */

# define ENC_LATIN1	0x200	    /* Latin1 */
# define ENC_LATIN9	0x400	    /* Latin9 */
# define ENC_MACROMAN	0x800	    /* Mac Roman (not Macro Man! :-) */

# define DBCS_JPN	932	/* japan */
# define DBCS_JPNU	9932	/* euc-jp */
# define DBCS_KOR	949	/* korea */
# define DBCS_KORU	9949	/* euc-kr */
# define DBCS_CHS	936	/* chinese */
# define DBCS_CHSU	9936	/* euc-cn */
# define DBCS_CHT	950	/* taiwan */
# define DBCS_CHTU	9950	/* euc-tw */
# define DBCS_2BYTE	1	/* 2byte- */
# define DBCS_DEBUG	-1

#define CONV_NONE		0
#define CONV_TO_UTF8		1
#define CONV_9_TO_UTF8		2
#define CONV_TO_LATIN1		3
#define CONV_TO_LATIN9		4
#define CONV_ICONV		5
#ifdef WIN3264
# define CONV_CODEPAGE		10	/* codepage -> codepage */
#endif
#ifdef MACOS_X
# define CONV_MAC_LATIN1	20
# define CONV_LATIN1_MAC	21
# define CONV_MAC_UTF8		22
# define CONV_UTF8_MAC		23
#endif


int enc_canon_props(const char *name);
int convert_setup_ext(vimconv_T *vcp, const char *from, int from_unicode_is_utf8, const char *to, int to_unicode_is_utf8);
int convert_setup(vimconv_T *vcp, const char *from, const char *to);
char *string_convert_ext(vimconv_T *vcp, char *ptr, int *lenp, int *unconvlenp);
char *string_convert(vimconv_T *vcp, char *ptr, int *lenp);
void conv_init();
void conv_cleanup();

#endif /* CONV_H */
