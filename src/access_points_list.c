/**
 * @file access_points_list.c
 * @author TheSomeMan
 * @date 2020-11-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "access_points_list.h"
#include <string.h>

ACCESS_POINTS_LIST_STATIC
void
ap_list_clear_wifi_ap_record(wifi_ap_record_t *p_wifi_ap)
{
    memset(p_wifi_ap, 0, sizeof(*p_wifi_ap));
}

ACCESS_POINTS_LIST_STATIC
void
ap_list_clear_identical_ap(wifi_ap_record_t *p_ap_src, wifi_ap_record_t *p_ap_dst)
{
    /* same SSID, different auth mode is skipped */
    if ((0 == strcmp((const char *)p_ap_src->ssid, (const char *)p_ap_dst->ssid))
        && (p_ap_src->authmode == p_ap_dst->authmode))
    {
        /* save the rssi for the display */
        if (p_ap_src->rssi < p_ap_dst->rssi)
        {
            p_ap_src->rssi = p_ap_dst->rssi;
        }
        ap_list_clear_wifi_ap_record(p_ap_dst);
    }
}

ACCESS_POINTS_LIST_STATIC
void
ap_list_clear_identical_aps(wifi_ap_record_t *p_arr_of_ap, const number_wifi_access_points_t num_aps)
{
    if (0 == num_aps)
    {
        return;
    }
    for (uint32_t i = 0; i < (num_aps - 1); ++i)
    {
        wifi_ap_record_t *p_ap = &p_arr_of_ap[i];
        if ('\0' == p_ap->ssid[0])
        {
            continue; /* skip the previously removed APs */
        }
        for (uint32_t j = i + 1; j < num_aps; ++j)
        {
            ap_list_clear_identical_ap(p_ap, &p_arr_of_ap[j]);
        }
    }
}

ACCESS_POINTS_LIST_STATIC
wifi_ap_record_t *
ap_list_find_first_free_slot(wifi_ap_record_t *p_arr_of_ap, const number_wifi_access_points_t num_aps)
{
    for (uint32_t j = 0; j < num_aps; ++j)
    {
        wifi_ap_record_t *p_ap = &p_arr_of_ap[j];
        if ('\0' == p_ap->ssid[0])
        {
            return p_ap;
        }
    }
    return NULL;
}

ACCESS_POINTS_LIST_STATIC
number_wifi_access_points_t
ap_list_reorder(wifi_ap_record_t *p_arr_of_ap, const number_wifi_access_points_t num_aps)
{
    /* reorder the list so APs follow each other in the list */
    number_wifi_access_points_t num_unique_aps = num_aps;
    wifi_ap_record_t *          p_first_free   = NULL;
    for (uint32_t i = 0; i < num_aps; ++i)
    {
        wifi_ap_record_t *p_ap = &p_arr_of_ap[i];
        /* skipping all that has no name */
        if ('\0' == p_ap->ssid[0])
        {
            /* mark the first free slot */
            if (NULL == p_first_free)
            {
                p_first_free = p_ap;
            }
            num_unique_aps--;
            continue;
        }
        if (NULL != p_first_free)
        {
            memcpy(p_first_free, p_ap, sizeof(wifi_ap_record_t));
            ap_list_clear_wifi_ap_record(p_ap);
            p_first_free = ap_list_find_first_free_slot(p_arr_of_ap, num_aps);
        }
    }
    return num_unique_aps;
}

number_wifi_access_points_t
ap_list_filter_unique(wifi_ap_record_t *p_arr_of_ap, const number_wifi_access_points_t num_aps)
{
    if (0 == num_aps)
    {
        return 0;
    }
    ap_list_clear_identical_aps(p_arr_of_ap, num_aps);
    return ap_list_reorder(p_arr_of_ap, num_aps);
}
