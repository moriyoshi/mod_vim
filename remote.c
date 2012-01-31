#include "conv.h"
#include "remote.h"

static char *serverConvert(VimRemotingClient *client, const char *client_enc, char *data, char **tofree);

#ifdef WIN3264
#include "remote_win32.c"
#elif defined(USE_X11)
#include "remote_x.c"
#endif

/*
 * If conversion is needed, convert "data" from "client_enc" to 'encoding' and
 * return an allocated string.  Otherwise return "data".
 * "*tofree" is set to the result when it needs to be freed later.
 */
static char *serverConvert(VimRemotingClient *client, const char *client_enc, char *data, char **tofree)
{
    char *res = data;

    *tofree = 0;
    if (client_enc && client->enc) {
        vimconv_T vimconv;

        vimconv.vc_type = CONV_NONE;
        if (!convert_setup(&vimconv, client_enc, client->enc) &&
                vimconv.vc_type != CONV_NONE)
        {
            res = string_convert(&vimconv, data, NULL);
            if (res == NULL)
                res = data;
            else
                *tofree = res;
        }
        convert_setup(&vimconv, NULL, NULL);
    }
    return res;
}


