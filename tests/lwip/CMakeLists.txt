set(LWIP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lwip/src)
set(LWIP_CONTRIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lwip/contrib)

set(LWIP_SOURCES
        "${LWIP_PATH}/core/altcp_alloc.c"
        "${LWIP_PATH}/core/altcp.c"
        "${LWIP_PATH}/core/altcp_tcp.c"
        "${LWIP_PATH}/core/def.c"
        "${LWIP_PATH}/core/dns.c"
        "${LWIP_PATH}/core/inet_chksum.c"
        "${LWIP_PATH}/core/init.c"
        "${LWIP_PATH}/core/ip.c"
        "${LWIP_PATH}/core/ipv4"
        "${LWIP_PATH}/core/ipv6"
        "${LWIP_PATH}/core/mem.c"
        "${LWIP_PATH}/core/memp.c"
        "${LWIP_PATH}/core/netif.c"
        "${LWIP_PATH}/core/pbuf.c"
        "${LWIP_PATH}/core/raw.c"
        "${LWIP_PATH}/core/stats.c"
        "${LWIP_PATH}/core/sys.c"
        "${LWIP_PATH}/core/tcp.c"
        "${LWIP_PATH}/core/tcp_in.c"
        "${LWIP_PATH}/core/tcp_out.c"
        "${LWIP_PATH}/core/timeouts.c"
        "${LWIP_PATH}/core/udp.c"

        "${LWIP_PATH}/core/ipv4/autoip.c"
        "${LWIP_PATH}/core/ipv4/dhcp.c"
        "${LWIP_PATH}/core/ipv4/etharp.c"
        "${LWIP_PATH}/core/ipv4/icmp.c"
        "${LWIP_PATH}/core/ipv4/igmp.c"
        "${LWIP_PATH}/core/ipv4/ip4_addr.c"
        "${LWIP_PATH}/core/ipv4/ip4.c"
        "${LWIP_PATH}/core/ipv4/ip4_frag.c"

        "${LWIP_PATH}/core/ipv6/dhcp6.c"
        "${LWIP_PATH}/core/ipv6/ethip6.c"
        "${LWIP_PATH}/core/ipv6/icmp6.c"
        "${LWIP_PATH}/core/ipv6/inet6.c"
        "${LWIP_PATH}/core/ipv6/ip6_addr.c"
        "${LWIP_PATH}/core/ipv6/ip6.c"
        "${LWIP_PATH}/core/ipv6/ip6_frag.c"
        "${LWIP_PATH}/core/ipv6/mld6.c"
        "${LWIP_PATH}/core/ipv6/nd6.c"

        "${LWIP_PATH}/api/api_lib.c"
        "${LWIP_PATH}/api/api_msg.c"
        "${LWIP_PATH}/api/err.c"
        "${LWIP_PATH}/api/if_api.c"
        "${LWIP_PATH}/api/netbuf.c"
        "${LWIP_PATH}/api/netdb.c"
        "${LWIP_PATH}/api/netifapi.c"
        "${LWIP_PATH}/api/sockets.c"
        "${LWIP_PATH}/api/tcpip.c"

        "${LWIP_PATH}/netif/bridgeif.c"
        "${LWIP_PATH}/netif/bridgeif_fdb.c"
        "${LWIP_PATH}/netif/ethernet.c"
        "${LWIP_PATH}/netif/lowpan6_ble.c"
        "${LWIP_PATH}/netif/lowpan6.c"
        "${LWIP_PATH}/netif/lowpan6_common.c"
        "${LWIP_PATH}/netif/slipif.c"
        "${LWIP_PATH}/netif/zepif.c"

        "${LWIP_PATH}/netif/ppp/auth.c"
        "${LWIP_PATH}/netif/ppp/ccp.c"
        "${LWIP_PATH}/netif/ppp/demand.c"
        "${LWIP_PATH}/netif/ppp/eap.c"
        "${LWIP_PATH}/netif/ppp/ecp.c"
        "${LWIP_PATH}/netif/ppp/eui64.c"
        "${LWIP_PATH}/netif/ppp/fsm.c"
        "${LWIP_PATH}/netif/ppp/chap-md5.c"
        "${LWIP_PATH}/netif/ppp/chap_ms.c"
        "${LWIP_PATH}/netif/ppp/chap-new.c"
        "${LWIP_PATH}/netif/ppp/ipcp.c"
        "${LWIP_PATH}/netif/ppp/ipv6cp.c"
        "${LWIP_PATH}/netif/ppp/lcp.c"
        "${LWIP_PATH}/netif/ppp/magic.c"
        "${LWIP_PATH}/netif/ppp/mppe.c"
        "${LWIP_PATH}/netif/ppp/multilink.c"
        "${LWIP_PATH}/netif/ppp/pppapi.c"
        "${LWIP_PATH}/netif/ppp/ppp.c"
        "${LWIP_PATH}/netif/ppp/pppcrypt.c"
        "${LWIP_PATH}/netif/ppp/pppoe.c"
        "${LWIP_PATH}/netif/ppp/pppol2tp.c"
        "${LWIP_PATH}/netif/ppp/pppos.c"
        "${LWIP_PATH}/netif/ppp/upap.c"
        "${LWIP_PATH}/netif/ppp/utils.c"
        "${LWIP_PATH}/netif/ppp/vj.c"

        "${LWIP_PATH}/netif/ppp/polarssl/arc4.c"
        "${LWIP_PATH}/netif/ppp/polarssl/des.c"
        "${LWIP_PATH}/netif/ppp/polarssl/md4.c"
        "${LWIP_PATH}/netif/ppp/polarssl/md5.c"
        "${LWIP_PATH}/netif/ppp/polarssl/sha1.c"
)

set(LWIP_CONTRIB
        "${LWIP_CONTRIB_PATH}/ports/freertos/sys_arch.c"
        "${LWIP_CONTRIB_PATH}/ports/unix/port/netif/sio.c"
)

include_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/extra"
        "${CMAKE_CURRENT_SOURCE_DIR}/lwip/src/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/lwip/contrib/ports/freertos/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/lwip/contrib/ports/unix/port/include"
)

add_definitions(-DDEFAULT_THREAD_STACKSIZE=1024 -DTCPIP_THREAD_STACKSIZE=1024 -g3 -ggdb)

add_library(lwip STATIC
        ${LWIP_SOURCES}
        ${LWIP_CONTRIB}
        "${CMAKE_CURRENT_SOURCE_DIR}/extra/tapif.c"
)
