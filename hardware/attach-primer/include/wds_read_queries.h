#ifndef LILAC_WDS_READ_QUERIES_H
#define LILAC_WDS_READ_QUERIES_H

#include "qmi_client.h"
#include "wireless_data_service_v01.h"

typedef qmi_client_error_type (*wds_send_fn)(qmi_client_type, unsigned int,
    void *, unsigned int, void *, unsigned int, unsigned int);
typedef int32_t (*wds_size_fn)(qmi_idl_service_object_type,
    qmi_idl_type_of_message_type, uint16_t, uint32_t *);

/* Four fixed getters only. Output directory is already opened privately.
 * Nonzero on any failed query or snapshot. Never retries or writes modem state.
 */
int wds_read_queries(qmi_client_type client, qmi_idl_service_object_type service,
                     wds_send_fn send, wds_size_fn get_size, int directory_fd);
#endif
