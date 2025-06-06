#ifndef FILE_WRITING_H_
#define FILE_WRITING_H_

#include "esp_wifi_types.h"
#include "lwip/err.h"
#include "scanner.h"
#include <time.h>

extern err_t record_single_port_data_to_file(const time_t time, ipv4_info * ipv4_info, uint8_t * ports);
extern err_t record_ipv4_list_data_to_file(const time_t time, ipv4_list * ipv4_list);
extern err_t record_ap_records_data_to_file(const time_t time, wifi_ap_record_t * ap_records);

#endif // FILE_WRITING_H_
