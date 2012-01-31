/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved        by Bram Moolenaar
 * X command server by Flemming Madsen
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 *
 * if_xcmdsrv.c: Functions for passing commands through an X11 display.
 *
 */

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <httpd.h>
#include <http_log.h>
#include "ga.h"
#include "utils.h"

#ifndef HAVE_SELECT
#include <poll.h>
#else
#include <sys/types.h>
#include <sys/select.h>
#endif

/*
 * This file provides procedures that implement the command server
 * functionality of Vim when in contact with an X11 server.
 *
 * Adapted from TCL/TK's send command  in tkSend.c of the tk 3.6 distribution.
 * Adapted for use in Vim by Flemming Madsen. Protocol changed to that of tk 4
 */

/*
 * Copyright (c) 1989-1993 The Regents of the University of California.
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */


/*
 * When a result is being awaited from a sent command, one of
 * the following structures is present on a list of all outstanding
 * sent commands.  The information in the structure is used to
 * process the result when it arrives.  You're probably wondering
 * how there could ever be multiple outstanding sent commands.
 * This could happen if Vim instances invoke each other recursively.
 * It's unlikely, but possible.
 */

typedef struct VimRemotingClient_PendingCommand {
    /* Serial number expected in result. */
    int serial;
    /* Result Code. 0 is OK */
    int code;
    /* String result for command (malloc'ed).
     * NULL means command still pending. */
    char *result;
    /* Next in list of all outstanding commands.
     * NULL means end of list. */
    struct VimRemotingClient_PendingCommand *nextPtr;
} VimRemotingClient_PendingCommand;

#define MAX_PROP_WORDS 100000

typedef struct VimRemotingClient_ServerReply {
    Window  id;
    garray_T strings;
} VimRemotingClient_ServerReply;

enum VimRemotingClient_ServerReplyOp {
    SROP_Find,
    SROP_Add,
    SROP_Delete
};

typedef int (*VimRemotingClient_EndCond)(void *);

/* Private variables for the "server" functionality */
struct VimRemotingClient {
    server_rec *server_rec;
    const char *vim_version;
    const char *enc;

    Display *dpy;
    Window window;

    /* Running count of sent commands.
     * Used to give each command a different serial number.
     */
    int serial;

    /* List of all commands currentlybeing waited for. */
    VimRemotingClient_PendingCommand *pendingCommands;

    Atom registryProperty;
    Atom commProperty;
    Atom vimProperty;

    int got_x_error;
    int got_int;
};

typedef struct VimRemotingClient_WaitForReplyParams {
    VimRemotingClient *client;
    Window w;
    VimRemotingClient_ServerReply *result;
} VimRemotingClient_WaitForReplyParams;

static char *empty_prop = (char *)"";        /* empty getRegProp() result */
static garray_T serverReply = { 0, 0, 0, 0, 0 };

static const char tls_key[] = "MOD_VIM";

/* current client - must be guarded with XLockDisplay() / XUnlockDisplay() */
static VimRemotingClient *currentClient;
static XErrorHandler oldErrorHandler;

/*
 * Another X Error handler, just used to check for errors.
 */
static int x_error_check(Display *dpy, XErrorEvent *error_event)
{
    currentClient->got_x_error = TRUE;
    return 0;
}

static void prologue(VimRemotingClient *client)
{
    XLockDisplay(client->dpy);
    assert(!currentClient);
    client->got_x_error = FALSE;

    currentClient = client;
    oldErrorHandler = XSetErrorHandler(x_error_check);
}

static void epilogue(VimRemotingClient *client)
{
    XSetErrorHandler(oldErrorHandler);
    currentClient = NULL;
    XUnlockDisplay(client->dpy);
}

static VimRemotingClient_ServerReply *findReply(VimRemotingClient *client, Window w, enum VimRemotingClient_ServerReplyOp op)
{
    VimRemotingClient_ServerReply *p;
    VimRemotingClient_ServerReply e;
    int i;

    p = (VimRemotingClient_ServerReply *) serverReply.ga_data;

    i = 0;
    while (i < serverReply.ga_len) {
        if (p->id == w)
            break;
        i++, p++;
    }

    if (i >= serverReply.ga_len)
        p = NULL;

    if (p == NULL && op == SROP_Add)
    {
        if (serverReply.ga_growsize == 0)
            ga_init2(&serverReply, sizeof(VimRemotingClient_ServerReply), 1);
        if (ga_grow(&serverReply, 1) == OK)
        {
            p = ((VimRemotingClient_ServerReply *) serverReply.ga_data)
                + serverReply.ga_len;
            e.id = w;
            ga_init2(&e.strings, 1, 100);
            memmove(p, &e, sizeof(e));
            serverReply.ga_len++;
        }
    }
    else if (p != NULL && op == SROP_Delete)
    {
        ga_clear(&p->strings);
        memmove(p, p + 1, (serverReply.ga_len - i - 1) * sizeof(*p));
        serverReply.ga_len--;
    }

    return p;
}


static int waitForPend(void *p)
{
    VimRemotingClient_PendingCommand *pending = p;
    return !!pending->result;
}

static int waitForReply(void *p)
{
    VimRemotingClient_WaitForReplyParams *params = p;
    return !!(params->result = findReply(params->client, params->w, SROP_Find));
}

/*
 * Append a given property to a given window, but set up an X error handler so
 * that if the append fails this procedure can return an error code rather
 * than having Xlib panic.
 * Return: 0 for OK, -1 for error
 */
static int appendPropCarefully(VimRemotingClient *client, Window window, Atom property, char *value, int length)
{
    XChangeProperty(client->dpy, window, property, XA_STRING, 8,
                    PropModeAppend, value, length);
    XSync(client->dpy, False);

    return client->got_x_error;
}

/*
 * Return TRUE if window "w" exists and has a "Vim" property on it.
 */
static int isWindowValid(VimRemotingClient *client, Window w)
{
    XErrorHandler   old_handler;
    Atom *plist;
    int numProp;
    int i;
    int retval = FALSE;

    plist = XListProperties(client->dpy, w, &numProp);
    XSync(client->dpy, False);
    if (plist == NULL || client->got_x_error)
        return FALSE;

    for (i = 0; i < numProp; i++) {
        if (plist[i] == client->vimProperty) {
            retval = TRUE;
            break;
        }
    }
    XFree(plist);
    return retval;
}

/*
 * Check if "str" looks like it had a serial number appended.
 * Actually just checks if the name ends in a digit.
 */
static int isSerialName(char *str)
{
    int len = strlen(str);
    return (len > 1 && isdigit(((unsigned char *)str)[len - 1]));
}

/*
 * Convert string to windowid.
 * Issue an error if the id is invalid.
 */
static Window serverStrToWin(VimRemotingClient *client, char *str)
{
    unsigned int id = None;

    sscanf((char *)str, "0x%x", &id);
    if (id == None) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, client->server_rec, "Invalid server id used: %s", str);
        return None;
    }

    return (Window)id;
}

/*
 * This procedure is invoked by the various X event loops throughout Vims when
 * a property changes on the communication window.  This procedure reads the
 * property and handles command requests and responses.
 */
static void serverEventProc(VimRemotingClient *client, XEvent *eventPtr)
{
    unsigned char *propInfo = NULL;
    char *p;
    int  result, actualFormat, code;
    unsigned long numItems, bytesAfter;
    Atom actualType;
    char *tofree;

    if (eventPtr != NULL) {
        if (eventPtr->xproperty.atom != client->commProperty
                || eventPtr->xproperty.state != PropertyNewValue)
            return;
    }

    /*
     * Read the comm property and delete it.
     */
    result = XGetWindowProperty(
            client->dpy, client->window, client->commProperty, 0L,
            (long)MAX_PROP_WORDS, True, XA_STRING, &actualType,
            &actualFormat, &numItems, &bytesAfter, &propInfo);

    /* If the property doesn't exist or is improperly formed then ignore it. */
    if (result != Success || actualType != XA_STRING || actualFormat != 8) {
        if (propInfo)
            XFree(propInfo);
        return;
    }

    /*
     * Several commands and results could arrive in the property at
     * one time;  each iteration through the outer loop handles a
     * single command or result.
     */
    for (p = (char *)propInfo; (p - (char *)propInfo) < numItems; ) {
        /*
         * Ignore leading NULs; each command or result starts with a
         * NUL so that no matter how badly formed a preceding command
         * is, we'll be able to tell that a new command/result is
         * starting.
         */
        if (*p == 0) {
            p++;
            continue;
        }

        if (*p == 'r' && p[1] == 0) {
            int                    serial, gotSerial;
            char            *res;
            VimRemotingClient_PendingCommand  *pcPtr;
            char            *enc;

            /*
             * This is a reply to some command that we sent out.  Iterate
             * over all of its options.  Stop when we reach the end of the
             * property or something that doesn't look like an option.
             */
            p += 2;
            gotSerial = 0;
            res = (char *)"";
            code = 0;
            enc = NULL;
            while ((p - (char *)propInfo) < numItems && *p == '-') {
                switch (p[1]) {
                    case 'r':
                        if (p[2] == ' ')
                            res = p + 3;
                        break;
                    case 'E':
                        if (p[2] == ' ')
                            enc = p + 3;
                        break;
                    case 's':
                        if (sscanf((char *)p + 2, " %d", &serial) == 1)
                            gotSerial = 1;
                        break;
                    case 'c':
                        if (sscanf((char *)p + 2, " %d", &code) != 1)
                            code = 0;
                        break;
                }
                while (*p != 0)
                    p++;
                p++;
            }

            if (!gotSerial)
                continue;

            /*
             * Give the result information to anyone who's
             * waiting for it.
             */
            for (pcPtr = client->pendingCommands; pcPtr != NULL; pcPtr = pcPtr->nextPtr) {
                if (serial != pcPtr->serial || pcPtr->result != NULL)
                    continue;

                pcPtr->code = code;
                if (res != NULL) {
                    res = serverConvert(client, enc, res, &tofree);
                    if (tofree == NULL)
                        res = strdup(res);
                    pcPtr->result = res;
                }
                else
                    pcPtr->result = strdup((char *)"");
                break;
            }
        } else if (*p == 'n' && p[1] == 0) {
            Window        win = 0;
            unsigned int u;
            int gotWindow;
            char *str;
            VimRemotingClient_ServerReply *r;
            char        *enc;

            /*
             * This is a (n)otification.  Sent with serverreply_send in VimL.
             * Execute any autocommand and save it for later retrieval
             */
            p += 2;
            gotWindow = 0;
            str = (char *)"";
            enc = NULL;
            while ((p - (char *)propInfo) < numItems && *p == '-') {
                switch (p[1]) {
                    case 'n':
                        if (p[2] == ' ')
                            str = p + 3;
                        break;
                    case 'E':
                        if (p[2] == ' ')
                            enc = p + 3;
                        break;
                    case 'w':
                        if (sscanf((char *)p + 2, " %x", &u) == 1) {
                            win = u;
                            gotWindow = 1;
                        }
                        break;
                }
                while (*p != 0)
                    p++;
                p++;
            }

            if (!gotWindow)
                continue;
            str = serverConvert(client, enc, str, &tofree);
            if ((r = findReply(client, win, SROP_Add)) != NULL) {
                ga_concat(&(r->strings), str);
                ga_append(&(r->strings), '\0');
            }
            free(tofree);
        } else {
            /*
             * Didn't recognize this thing.  Just skip through the next
             * null character and try again.
             * Even if we get an 'r'(eply) we will throw it away as we
             * never specify (and thus expect) one
             */
            while (*p != 0)
                p++;
            p++;
        }
    }
    XFree(propInfo);
}

/*
 * Read the registry property.  Delete it when it's formatted wrong.
 * Return the property in "regPropp".  "empty_prop" is used when it doesn't
 * exist yet.
 * Return OK when successful.
 */
static int getRegProp(VimRemotingClient *client, unsigned char **regPropp, unsigned long *numItemsp)
{
    int result, actualFormat;
    unsigned long  bytesAfter;
    Atom actualType;
    Window rootWindow = RootWindow(client->dpy, 0);

    *regPropp = NULL;

    {
        result = XGetWindowProperty(
                client->dpy, rootWindow,
                client->registryProperty, 0L,
                (long)MAX_PROP_WORDS, False,
                XA_STRING, &actualType,
                &actualFormat, numItemsp, &bytesAfter,
                regPropp);

        XSync(client->dpy, FALSE);

        if (client->got_x_error)
            return -1;
    }

    if (actualType == None) {
        /* No prop yet. Logically equal to the empty list */
        *numItemsp = 0;
        *regPropp = empty_prop;
        return 0;
    }

    /* If the property is improperly formed, then delete it. */
    if (result != Success || actualFormat != 8 || actualType != XA_STRING) {
        if (*regPropp != NULL)
            XFree(*regPropp);

        XDeleteProperty(client->dpy, rootWindow, client->registryProperty);

        ap_log_error(APLOG_MARK, APLOG_ERR, 0, client->server_rec, "VIM instance registry property is badly formed.  Deleted!");

        return -1;
    }

    return 0;
}


static int pollFor(int fd, int msec)
{
#ifndef HAVE_SELECT
    struct pollfd   fds;

    fds.fd = fd;
    fds.events = POLLIN;
    return poll(&fds, 1, msec) < 0;
#else
    fd_set fds;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = msec * 1000;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    return select(fd + 1, &fds, NULL, NULL, &tv) < 0;
#endif
}

/*
 * Enter a loop processing X events & polling chars until we see a result
 */
static void serverWait(VimRemotingClient *client, Window w, VimRemotingClient_EndCond endCond, void *endData, int seconds)
{
    time_t            start;
    time_t            now;
    time_t            lastChk = 0;
    XEvent            event;
    XPropertyEvent *e = (XPropertyEvent *)&event;
    int fd = ConnectionNumber(client->dpy);

    time(&start);
    while (!endCond(endData)) {
        time(&now);
        /* Just look out for the answer without calling back into Vim */
        pollFor(fd, ((start + seconds) - now) * 1000);
        if (!isWindowValid(client, w))
            break;
        while (XEventsQueued(client->dpy, QueuedAfterReading) > 0) {
            XNextEvent(client->dpy, &event);
            if (event.type == PropertyNotify &&
                    e->window == client->window) {
                serverEventProc(client, &event);
            }
        }
    }
}

/*
 * Fetch a list of all the Vim instance names currently registered for the
 * display.
 *
 * Returns a newline separated list in allocated memory or NULL.
 */
char *serverGetVimNames(VimRemotingClient *client)
{
    unsigned char *regProp;
    char *entry;
    char *p;
    unsigned long numItems;
    unsigned int        w;
    garray_T        ga;

    ga_init2(&ga, 1, 100);

    /*
     * Read the registry property.
     */
    if (getRegProp(client, &regProp, &numItems))
        return NULL;

    /*
     * Scan all of the names out of the property.
     */
    ga_init2(&ga, 1, 100);
    for (p = (char *)regProp; (p - (char *)regProp) < numItems; p++) {
        entry = p;
        while (*p != 0 && !isspace(*(unsigned char *)p))
            p++;
        if (*p != 0)
        {
            w = None;
            sscanf((char *)entry, "%x", &w);
            if (isWindowValid(client, (Window)w)) {
                ga_concat(&ga, p + 1);
                ga_concat(&ga, (char *)"\n");
            }
            while (*p != 0)
                p++;
        }
    }
    if ((char *)regProp != empty_prop)
        XFree(regProp);
    ga_append(&ga, '\0');
    return ga.ga_data;
}

/*
 * Send a reply string (notification) to client with id "name".
 * Return -1 if the window is invalid.
 */
int serverSendReply(VimRemotingClient *client, char *name, char *str)
{
    char    *property;
    int     length;
    int     res = 0;
    Window  win = serverStrToWin(client, name);

    if (!isWindowValid(client, win))
        return -1;

#ifdef FEAT_MBYTE
    length = strlen(p_enc) + strlen(str) + 14;
#else
    length = strlen(str) + 10;
#endif
    if (!(property = malloc((unsigned)length + 30))) {
        return -1;
    }

#ifdef FEAT_MBYTE
    sprintf(property, "%cn%c-E %s%c-n %s%c-w %x",
            0, 0, p_enc, 0, str, 0, (unsigned int)client->window);
#else
    sprintf(property, "%cn%c-n %s%c-w %x",
            0, 0, str, 0, (unsigned int)client->window);
#endif
        /* Add length of what "%x" resulted in. */
    length += strlen(property + length);
    res = appendPropCarefully(client, win, client->commProperty, property, length + 1);
    free(property);
    return res;
}

/*
 * Wait for replies from id (win)
 * Return 0 and the malloc'ed string when a reply is available.
 * Return -1 if the window becomes invalid while waiting.
 */
int serverReadReply(VimRemotingClient *client, Window w, char **str)
{
    int len;
    char *s;
    VimRemotingClient_ServerReply *p;
    VimRemotingClient_WaitForReplyParams params = { client, w, NULL };

    serverWait(client, w, waitForReply, &params, -1);

    if (params.result && params.result->strings.ga_len > 0) {
        *str = strdup(params.result->strings.ga_data);
        len = strlen(*str) + 1;
        if (len < params.result->strings.ga_len) {
            s = (char *) params.result->strings.ga_data;
            memmove(s, s + len, params.result->strings.ga_len - len);
            params.result->strings.ga_len -= len;
        } else {
            /* Last string read.  Remove from list */
            ga_clear(&params.result->strings);
            findReply(client, w, SROP_Delete);
        }
        return 0;
    }
    return -1;
}

/*
 * Check for replies from id (win).
 * Return TRUE and a non-malloc'ed string if there is.  Else return FALSE.
 */
static int serverPeekReply(VimRemotingClient *client, Window win, char **str)
{
    VimRemotingClient_ServerReply *p;

    if ((p = findReply(client, win, SROP_Find)) != NULL &&
            p->strings.ga_len > 0) {
        if (str != NULL)
            *str = p->strings.ga_data;
        return 1;
    }
    if (!isWindowValid(client, win))
        return -1;
    return 0;
}

/*
 * Given a server name, see if the name exists in the registry for a
 * particular display.
 *
 * If the given name is registered, return the ID of the window associated
 * with the name.  If the name isn't registered, then return 0.
 *
 * Side effects:
 *        If the registry property is improperly formed, then it is deleted.
 *        If "delete" is non-zero, then if the named server is found it is
 *        removed from the registry property.
 */
static Window lookupName(VimRemotingClient *client, const char *name)
{
    unsigned char *regProp;
    char *entry;
    char *p;
    unsigned long numItems;
    unsigned int returnValue;

    /*
     * Read the registry property.
     */
    if (getRegProp(client, &regProp, &numItems))
        return 0;

    /*
     * Scan the property for the desired name.
     */
    returnValue = (unsigned int)None;
    entry = NULL;        /* Not needed, but eliminates compiler warning. */
    for (p = (char *)regProp; (p - (char *)regProp) < numItems; ) {
        entry = p;
        while (*p != 0 && !isspace(*(unsigned char *)p))
            p++;
        if (*p != 0 && strcasecmp(name, p + 1) == 0) {
            sscanf((char *)entry, "%x", &returnValue);
            break;
        }
        while (*p != 0)
            p++;
        p++;
    }

    if ((char *)regProp != empty_prop)
        XFree(regProp);
    return (Window)returnValue;
}

/*
 * Delete any lingering occurrence of window id.  We promise that any
 * occurrence is not ours since it is not yet put into the registry (by us)
 *
 * This is necessary in the following scenario:
 * 1. There is an old windowid for an exit'ed vim in the registry
 * 2. We get that id for our commWindow but only want to send, not register.
 * 3. The window will mistakenly be regarded valid because of own commWindow
 */
static void deleteAnyLingerer(VimRemotingClient *client)
{
    unsigned char *regProp;
    char *entry = NULL;
    char *p;
    unsigned long numItems;
    unsigned int wwin;

    /*
     * Read the registry property.
     */
    if (getRegProp(client, &regProp, &numItems))
        return;

    /* Scan the property for the window id.  */
    for (p = (char *)regProp; (p - (char *)regProp) < numItems; ) {
        if (*p != 0) {
            sscanf((char *)p, "%x", &wwin);
            if ((Window)wwin == client->window) {
                int lastHalf;

                /* Copy down the remainder to delete entry */
                entry = p;
                while (*p != 0)
                    p++;
                p++;
                lastHalf = numItems - (p - (char *)regProp);
                if (lastHalf > 0)
                    memmove(entry, p, lastHalf);
                numItems = (entry - (char *)regProp) + lastHalf;
                p = entry;
                continue;
            }
        }
        while (*p != 0)
            p++;
        p++;
    }

    if (entry != NULL) {
        XChangeProperty(client->dpy, RootWindow(client->dpy, 0),
                        client->registryProperty, XA_STRING, 8,
                        PropModeReplace, regProp, p - (char *)regProp);
        XSync(client->dpy, False);
    }

    if ((char *)regProp != empty_prop)
        XFree(regProp);
}

/*
 * Send to an instance of Vim via the X display.
 * Returns 0 for OK, negative for an error.
 */
int serverSendToVim(VimRemotingClient *client, const char *name, const char *cmd, apr_size_t cmd_len, char **result)
{
    Window w;
    char *property;
    int length;
    int res;
    int n;
    VimRemotingClient_PendingCommand pending;

    if (result != NULL)
        *result = NULL;

    prologue(client);

    /*
     * Bind the server name to a communication window.
     *
     * Find any survivor with a serialno attached to the name if the
     * original registrant of the wanted name is no longer present.
     *
     * Delete any lingering names from dead editors.
     */
    do {
        w = lookupName(client, name);
        /* Check that the window is hot */
    } while (w != None && !isWindowValid(client, w));

    if (w == None) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, client->server_rec, "Failed to connect the server %s", name);
        epilogue(client);
        return -1;
    }

    /*
     * Send the command to target interpreter by appending it to the
     * comm window in the communication window.
     * Length must be computed exactly!
     */
#ifdef FEAT_MBYTE
    length = strlen(name) + strlen(p_enc) + cmd_len + 14;
#else
    length = strlen(name) + cmd_len + 10;
#endif
    property = (char *)malloc((unsigned)length + 30);

#ifdef FEAT_MBYTE
    n = sprintf((char *)property, "%c%c%c-n %s%c-E %s%c-s ",
                      0, result ? 'c' : 'k', 0, name, 0, p_enc, 0);
#else
    n = sprintf((char *)property, "%c%c%c-n %s%c-s ",
                      0, result ? 'c' : 'k', 0, name, 0);
#endif
    {
        memcpy(property + n, cmd, cmd_len);
        property[n + cmd_len] = '\0';
    }

    /* Add a back reference to our comm window */
    client->serial++;
    sprintf((char *)property + length, "%c-r %x %d",
            0, (unsigned int)client->window, client->serial);
    /* Add length of what "-r %x %d" resulted in, skipping the NUL. */
    length += strlen(property + length + 1) + 1;

    res = appendPropCarefully(client, w, client->commProperty, property, length + 1);

    free(property);

    if (res < 0) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, client->server_rec, "Failed to send command to the destination program");
        epilogue(client);
        return -1;
    }

    if (!result) {
        /* There is no answer for this - Keys are sent async */
        epilogue(client);
        return client->got_x_error;
    }

    /*
     * Register the fact that we're waiting for a command to
     * complete (this is needed by SendEventProc and by
     * AppendErrorProc to pass back the command's results).
     */
    pending.serial = client->serial;
    pending.code = 0;
    pending.result = NULL;
    pending.nextPtr = client->pendingCommands;
    client->pendingCommands = &pending;

    serverWait(client, w, waitForPend, &pending, 600);

    /*
     * Unregister the information about the pending command
     * and return the result.
     */
    if (client->pendingCommands == &pending) {
        client->pendingCommands = pending.nextPtr;
    } else {
        VimRemotingClient_PendingCommand *pcPtr;
        for (pcPtr = client->pendingCommands; pcPtr; pcPtr = pcPtr->nextPtr) {
            if (pcPtr->nextPtr == &pending) {
                pcPtr->nextPtr = pending.nextPtr;
                break;
            }
        }
    }
    if (result)
        *result = pending.result;
    else
        free(pending.result);

    epilogue(client);
    return pending.code == 0 ? 0 : -1;
}

static int VimRemotingClient_init_internal(VimRemotingClient *client)
{
    prologue(client);

    client->commProperty = XInternAtom(client->dpy, "Comm", False);
    client->vimProperty = XInternAtom(client->dpy, "Vim", False);
    client->registryProperty = XInternAtom(client->dpy, "VimRegistry", False);

    client->window = XCreateSimpleWindow(
            client->dpy, XDefaultRootWindow(client->dpy),
            getpid(), 0, 10, 10, 0,
            WhitePixel(client->dpy, DefaultScreen(client->dpy)),
            WhitePixel(client->dpy, DefaultScreen(client->dpy)));
    XSelectInput(client->dpy, client->window, PropertyChangeMask);

    /* WARNING: Do not step through this while debugging, it will hangup
     * the X server! */
    XGrabServer(client->dpy);
    deleteAnyLingerer(client);
    XUngrabServer(client->dpy);

    /* Make window recognizable as a vim window */
    XChangeProperty(
            client->dpy, client->window,
            client->vimProperty, XA_STRING,
            8, PropModeReplace, (char *)client->vim_version,
            (int)strlen(client->vim_version) + 1);

    XSync(client->dpy, False);

    epilogue(client);
    return client->got_x_error;
}

static void VimRemotingClient_destory(VimRemotingClient *client)
{
    prologue(client);
    XDestroyWindow(client->dpy, client->window);
    epilogue(client);
}

static int VimRemotingClient_init(VimRemotingClient *client, server_rec *server_rec, const char *vim_version, const char *enc, Display *dpy)
{
    client->server_rec = server_rec;
    client->vim_version = vim_version;
    client->enc = enc;
    client->dpy = dpy;
    client->window = None;
    client->serial = 0;
    client->pendingCommands = NULL;
    client->got_x_error = 0;
    client->got_int = 0;
    client->commProperty = None;
    client->registryProperty = None;
    client->vimProperty = None;

    return VimRemotingClient_init_internal(client);
}

static Display *dpy;

void VimRemotingClient_delete(VimRemotingClient *client)
{
    if (!client)
        return;
    VimRemotingClient_destory(client);
    free(client);
}

VimRemotingClient *VimRemotingClient_new(server_rec *server_rec, const char *vim_version, const char *enc, Display *dpy)
{
    VimRemotingClient *client = malloc(sizeof(*client));
    if (!client) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, server_rec, "Cannot allocate space for VimRemotingClient");
        return NULL;
    }
    if (VimRemotingClient_init(client, server_rec, vim_version, enc, dpy)) {
        free(client);
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, server_rec, "Cannot create a VimRemotingClient");
        return NULL;
    }
    return client;
}
