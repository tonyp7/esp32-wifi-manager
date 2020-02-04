if(IDF_VERSION_MAJOR GREATER_EQUAL 4)
    idf_component_register(SRCS src/dns_server.c src/http_server.c src/wifi_manager.c src/json.c ../../main/ethernet.c ../../main/settings.c
        REQUIRES log nvs_flash mdns wpa_supplicant lwip esp_http_server json
        INCLUDE_DIRS src ../../main/includes
        EMBED_FILES src/style.css src/jquery.gz src/code.js src/index.html src/ruuvi.html src/ruuvi.js src/ruuvi.css)
else()
    set(COMPONENT_SRCDIRS src)
    set(COMPONENT_ADD_INCLUDEDIRS src)
    set(COMPONENT_REQUIRES log nvs_flash mdns wpa_supplicant lwip esp_http_server json)
    set(COMPONENT_EMBED_FILES src/style.css src/jquery.gz src/code.js src/index.html)
    register_component()
endif()
