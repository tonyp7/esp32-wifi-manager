/**
 * @file access_points_list.h
 * @author TheSomeMan
 * @date 2020-11-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFI_MANAGER_ACCESS_POINTS_LIST_H
#define WIFI_MANAGER_ACCESS_POINTS_LIST_H

#include <stdint.h>
#include "esp_wifi_types.h"

#if !defined(RUUVI_TESTS_ACCESS_POINTS_LIST)
#define RUUVI_TESTS_ACCESS_POINTS_LIST (0)
#endif

#if RUUVI_TESTS_ACCESS_POINTS_LIST
#define ACCESS_POINTS_LIST_STATIC
#else
#define ACCESS_POINTS_LIST_STATIC static
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t number_wifi_access_points_t;

number_wifi_access_points_t
ap_list_filter_unique(wifi_ap_record_t *p_arr_of_ap, const number_wifi_access_points_t num_aps);

#if RUUVI_TESTS_ACCESS_POINTS_LIST

ACCESS_POINTS_LIST_STATIC
void
ap_list_clear_wifi_ap_record(wifi_ap_record_t *p_wifi_ap);

ACCESS_POINTS_LIST_STATIC
void
ap_list_clear_identical_ap(wifi_ap_record_t *p_ap_src, wifi_ap_record_t *p_ap_dst);

ACCESS_POINTS_LIST_STATIC
void
ap_list_clear_identical_aps(wifi_ap_record_t *p_arr_of_ap, const number_wifi_access_points_t num_aps);

ACCESS_POINTS_LIST_STATIC
wifi_ap_record_t *
ap_list_find_first_free_slot(wifi_ap_record_t *p_arr_of_ap, const number_wifi_access_points_t num_aps);

ACCESS_POINTS_LIST_STATIC
number_wifi_access_points_t
ap_list_reorder(wifi_ap_record_t *p_arr_of_ap, const number_wifi_access_points_t num_aps);

#endif // RUUVI_TESTS_ACCESS_POINTS_LIST

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_ACCESS_POINTS_LIST_H
