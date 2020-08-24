include(${CMAKE_CURRENT_LIST_DIR}/embed_files.txt)

if(IDF_VERSION_MAJOR GREATER_EQUAL 4)
    idf_component_register(
        SRCS
            src/dns_server.c
            src/http_server.c
            src/json.c
            src/json.h
            src/json_access_points.c
            src/json_access_points.h
            src/json_network_info.c
            src/json_network_info.h
            src/ruuvi_gwui_html.c
            src/ruuvi_gwui_html.h
            src/str_buf.c
            src/str_buf.h
            src/wifi_manager.c
            src/wifi_manager.h
            src/wifi_manager_defs.h
            ../../main/ethernet.c
            ../../main/settings.c
            ../../main/metrics.c
        REQUIRES
            log
            nvs_flash
            mdns
            wpa_supplicant
            lwip
            esp_http_server
            json
        INCLUDE_DIRS
            src
            ../../main/includes
        EMBED_FILES ${EMBED_FILES})
else()
    set(COMPONENT_SRCDIRS src)
    set(COMPONENT_ADD_INCLUDEDIRS src)
    set(COMPONENT_REQUIRES log nvs_flash mdns wpa_supplicant lwip esp_http_server json)
    set(COMPONENT_EMBED_FILES ${EMBED_FILES})
    register_component()
endif()
