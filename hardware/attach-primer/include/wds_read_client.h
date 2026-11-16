#ifndef LILAC_WDS_READ_CLIENT_H
#define LILAC_WDS_READ_CLIENT_H
#include "wds_read_queries.h"

struct wds_api {
    __typeof__(&wds_get_service_object_internal_v01) service;
    __typeof__(&qmi_idl_get_service_id) service_id;
    __typeof__(&qmi_idl_get_idl_version) major;
    __typeof__(&qmi_idl_get_idl_minor_version) minor;
    __typeof__(&qmi_client_get_service_list) list;
    __typeof__(&qmi_client_get_instance_id) instance;
    __typeof__(&qmi_client_init) init;
    __typeof__(&qmi_client_release) release;
    wds_send_fn send;
    wds_size_fn size;
};

/* Own client only; caller must enforce a deadline before loading vendor DSOs. */
int wds_read_client(const struct wds_api *api, int directory_fd);
int wds_read_deadline(unsigned seconds);
#endif
