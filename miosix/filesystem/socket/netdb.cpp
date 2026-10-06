/***************************************************************************
 *   Copyright (C) 2026 by Niccolò Betto                                   *
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

#include <config/miosix_settings.h>

#ifdef WITH_NETWORKING
#include "netdb.h"
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <lwip/api.h>
#include <lwip/dns.h>

namespace {
struct ResolverResult {
    addrinfo info;
    sockaddr_in address;
    // Optional input name comes after structure in the same allocation
};
} // namespace

extern "C" int getaddrinfo(const char *name, const char *service,
                           const addrinfo *req, addrinfo **result) {
    if (!result)
        return EAI_FAIL;
    *result = nullptr;
    if (!name && !service)
        return EAI_NONAME;

    int flags = 0;
    int type = 0;
    int protocol = 0;
    if (req) {
        flags = req->ai_flags;
        type = req->ai_socktype;
        protocol = req->ai_protocol;

        if (req->ai_family != AF_UNSPEC && req->ai_family != AF_INET)
            return EAI_FAMILY;
    }

    if (flags & ~(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV))
        return EAI_BADFLAGS;
    if ((flags & AI_CANONNAME) && !name)
        return EAI_BADFLAGS;
    if (type && type != SOCK_STREAM && type != SOCK_DGRAM)
        return EAI_SOCKTYPE;
    if (protocol && protocol != IPPROTO_TCP && protocol != IPPROTO_UDP)
        return EAI_SERVICE;

    if (!type)
        type = protocol == IPPROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;

    const int expectedProtocol = type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;
    if (protocol && protocol != expectedProtocol)
        return EAI_SOCKTYPE;
    protocol = expectedProtocol;

    unsigned int port = 0;
    if (service) {
        if (!*service)
            return (flags & AI_NUMERICSERV) ? EAI_NONAME : EAI_SERVICE;
        for (const char *p = service; *p; ++p) {
            if (*p < '0' || *p > '9')
                return (flags & AI_NUMERICSERV) ? EAI_NONAME : EAI_SERVICE;
            port = port * 10 + (*p - '0');
            if (port > 65535)
                return EAI_SERVICE;
        }
    }

    size_t nameLength = 0;
    in_addr address{};
    if (!name) {
        address.s_addr =
            htonl(flags & AI_PASSIVE ? INADDR_ANY : INADDR_LOOPBACK);
    } else {
        // Bound the name scan before passing it to lwIP or allocating storage
        while (nameLength < 256 && name[nameLength])
            ++nameLength;
        if (!nameLength || nameLength >= 256)
            return EAI_NONAME;
        if (inet_pton(AF_INET, name, &address) != 1) {
            if (flags & AI_NUMERICHOST)
                return EAI_NONAME;
#if LWIP_DNS && LWIP_IPV4
            ip_addr_t resolved;
            const err_t error = netconn_gethostbyname_addrtype(
                name, &resolved, NETCONN_DNS_IPV4);
            if (error == ERR_MEM)
                return EAI_MEMORY;
            if (error == ERR_TIMEOUT)
                return EAI_AGAIN;
            if (error != ERR_OK)
                return EAI_FAIL;
            address.s_addr = ip4_addr_get_u32(ip_2_ip4(&resolved));
#else
            return EAI_FAIL;
#endif
        }
    }

    const size_t extra = (flags & AI_CANONNAME) ? nameLength + 1 : 0;
    auto *storage = static_cast<ResolverResult *>(
        std::malloc(sizeof(ResolverResult) + extra));
    if (!storage)
        return EAI_MEMORY;
    std::memset(storage, 0, sizeof(*storage));

    storage->address.sin_len = sizeof(sockaddr_in);
    storage->address.sin_family = AF_INET;
    storage->address.sin_port = htons(static_cast<uint16_t>(port));
    storage->address.sin_addr = address;
    addrinfo &info = storage->info;
    info.ai_flags = flags;
    info.ai_family = AF_INET;
    info.ai_socktype = type;
    info.ai_protocol = protocol;
    info.ai_addrlen = sizeof(sockaddr_in);
    info.ai_addr = reinterpret_cast<sockaddr *>(&storage->address);
    if (extra) {
        info.ai_canonname = reinterpret_cast<char *>(storage + 1);
        std::memcpy(info.ai_canonname, name, extra);
    }
    *result = &info;
    return 0;
}

extern "C" void freeaddrinfo(addrinfo *ai) {
    while (ai) {
        addrinfo *next = ai->ai_next;
        std::free(ai);
        ai = next;
    }
}

extern "C" const char *gai_strerror(int error) {
    switch (error) {
    case 0:
        return "Success";
    case EAI_NONAME:
        return "Invalid or unavailable host name";
    case EAI_SERVICE:
        return "Unsupported service or protocol";
    case EAI_FAIL:
        return "Name resolution failed; check the hostname and network DNS "
               "configuration";
    case EAI_MEMORY:
        return "Not enough memory for name resolution";
    case EAI_FAMILY:
        return "Unsupported address family";
    case EAI_BADFLAGS:
        return "Unsupported resolver flags";
    case EAI_SOCKTYPE:
        return "Unsupported socket type or protocol combination";
    case EAI_AGAIN:
        return "Name resolution temporarily unavailable";
    default:
        return "Unknown name resolution error";
    }
}
#endif // WITH_NETWORKING
