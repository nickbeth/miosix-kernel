# Copyright (C) 2026 by Niccolò Betto
#
# This program is free software; you can redistribute it and/or
# it under the terms of the GNU General Public License as published
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# As a special exception, if other files instantiate templates or use
# macros or inline functions from this file, or you compile this file
# and link it with other works to produce a work based on this file,
# this file does not by itself cause the resulting work to be covered
# by the GNU General Public License. However the source code for this
# file must still be made available in accordance with the GNU
# Public License. This exception does not invalidate any other
# why a work based on this file might be covered by the GNU General
# Public License.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>

# Source lists from lwIP 2.2.1 src/Filelists.cmake, sync when updating lwIP
# Copied here because that CMake file also does other unwanted things
set(LWIP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lwip)

set(lwipcore_SRCS
    ${LWIP_DIR}/src/core/init.c
    ${LWIP_DIR}/src/core/def.c
    ${LWIP_DIR}/src/core/dns.c
    ${LWIP_DIR}/src/core/inet_chksum.c
    ${LWIP_DIR}/src/core/ip.c
    ${LWIP_DIR}/src/core/mem.c
    ${LWIP_DIR}/src/core/memp.c
    ${LWIP_DIR}/src/core/netif.c
    ${LWIP_DIR}/src/core/pbuf.c
    ${LWIP_DIR}/src/core/raw.c
    ${LWIP_DIR}/src/core/stats.c
    ${LWIP_DIR}/src/core/sys.c
    ${LWIP_DIR}/src/core/altcp.c
    ${LWIP_DIR}/src/core/altcp_alloc.c
    ${LWIP_DIR}/src/core/altcp_tcp.c
    ${LWIP_DIR}/src/core/tcp.c
    ${LWIP_DIR}/src/core/tcp_in.c
    ${LWIP_DIR}/src/core/tcp_out.c
    ${LWIP_DIR}/src/core/timeouts.c
    ${LWIP_DIR}/src/core/udp.c
)

set(lwipcore4_SRCS
    ${LWIP_DIR}/src/core/ipv4/acd.c
    ${LWIP_DIR}/src/core/ipv4/autoip.c
    ${LWIP_DIR}/src/core/ipv4/dhcp.c
    ${LWIP_DIR}/src/core/ipv4/etharp.c
    ${LWIP_DIR}/src/core/ipv4/icmp.c
    ${LWIP_DIR}/src/core/ipv4/igmp.c
    ${LWIP_DIR}/src/core/ipv4/ip4_frag.c
    ${LWIP_DIR}/src/core/ipv4/ip4.c
    ${LWIP_DIR}/src/core/ipv4/ip4_addr.c
)

set(lwipapi_SRCS
    ${LWIP_DIR}/src/api/api_lib.c
    ${LWIP_DIR}/src/api/api_msg.c
    ${LWIP_DIR}/src/api/err.c
    ${LWIP_DIR}/src/api/if_api.c
    ${LWIP_DIR}/src/api/netbuf.c
    ${LWIP_DIR}/src/api/netdb.c
    ${LWIP_DIR}/src/api/netifapi.c
    ${LWIP_DIR}/src/api/sockets.c
    ${LWIP_DIR}/src/api/tcpip.c
)

set(lwipnetif_SRCS
    ${LWIP_DIR}/src/netif/ethernet.c
    ${LWIP_DIR}/src/netif/bridgeif.c
    ${LWIP_DIR}/src/netif/bridgeif_fdb.c
    ${LWIP_DIR}/src/netif/slipif.c
)

# Add network stack source files
target_sources(miosix PRIVATE
    ${lwipcore_SRCS}
    ${lwipcore4_SRCS}
    ${lwipnetif_SRCS}
    ${lwipapi_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/port/sys_arch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/network.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/stm32_ethif.cpp
)

# Add network stack include directories
target_include_directories(miosix PRIVATE
    ${LWIP_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/port/include
)
