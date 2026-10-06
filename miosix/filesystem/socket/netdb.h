/***************************************************************************
 *   Copyright (C) 2026 by Niccolò Betto                                   *
 *   Copyright (C) 2001-2004 by Swedish Institute of Computer Science      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef	_NETDB_H_
#define	_NETDB_H_

#include <stdint.h>
#include <sys/socket.h>

struct addrinfo {
    int               ai_flags;      /* Input flags. */
    int               ai_family;     /* Address family of socket. */
    int               ai_socktype;   /* Socket type. */
    int               ai_protocol;   /* Protocol of socket. */
    socklen_t         ai_addrlen;    /* Length of socket address. */
    struct sockaddr  *ai_addr;       /* Socket address of socket. */
    char             *ai_canonname;  /* Canonical name of service location. */
    struct addrinfo  *ai_next;       /* Pointer to next in list. */
};

/* input flags for struct addrinfo */
#define AI_PASSIVE      0x01
#define AI_CANONNAME    0x02
#define AI_NUMERICHOST  0x04
#define AI_NUMERICSERV  0x08

/** Errors used by the DNS API functions, h_errno can be one of them */
#define EAI_NONAME      200
#define EAI_SERVICE     201
#define EAI_FAIL        202
#define EAI_MEMORY      203
#define EAI_FAMILY      204
#define EAI_BADFLAGS    205
#define EAI_SOCKTYPE    206
#define EAI_AGAIN       207

#ifdef __cplusplus
extern "C" {
#endif

/* Translate name of a service location and/or a service name to set of
   socket addresses.

   This function is a possible cancellation point and therefore not
   marked with __THROW.  */
extern int getaddrinfo(const char *__restrict name,
                       const char *__restrict service,
                       const struct addrinfo *__restrict req,
                       struct addrinfo **__restrict result);

/* Free `addrinfo' structure AI including associated storage.  */
extern void freeaddrinfo(struct addrinfo *ai) __THROW;

/* Convert error return from getaddrinfo() to a string.  */
extern const char *gai_strerror(int ecode) __THROW;

#ifdef __cplusplus
}
#endif

#endif /* _NETDB_H_ */
