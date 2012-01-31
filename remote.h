#ifndef REMOTE_H
#define REMOTE_H

typedef struct VimRemotingClient VimRemotingClient;

void VimRemotingClient_delete(VimRemotingClient *client);

#ifdef WIN3264
#else
#include <httpd.h>
#include <X11/Intrinsic.h>

VimRemotingClient *VimRemotingClient_new(server_rec *server_rec, const char *vim_version, const char *enc, Display *dpy);
#endif

int serverSendToVim(VimRemotingClient *client, const char *name, const char *cmd, apr_size_t cmd_len, char **result);

#endif /* REMOTE_H */
