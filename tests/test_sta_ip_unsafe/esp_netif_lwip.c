// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <lwip/ip_addr.h>
#include "esp_netif.h"

char *
esp_ip4addr_ntoa(const esp_ip4_addr_t *addr, char *buf, int buflen)
{
    return ip4addr_ntoa_r((ip4_addr_t *)addr, buf, buflen);
}

uint32_t
esp_ip4addr_aton(const char *addr)
{
    return ipaddr_addr(addr);
}
