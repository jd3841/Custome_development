#include "sip_communication.h"
#include <stdio.h>
#include <string.h>

int sip_init(const sip_config_t *config)
{
    (void)config;
    return 0;
}

int sip_connect(void)
{
    return 0;
}

int sip_trigger_call(const char *destination_sip_uri)
{
    printf("[stub] sip_trigger_call('%s')\n", destination_sip_uri ? destination_sip_uri : "(null)");
    return 0;
}

int sip_start_recording(const char *recording_folder)
{
    printf("[stub] sip_start_recording('%s')\n", recording_folder ? recording_folder : "(null)");
    return 0;
}

int sip_stop_recording(void)
{
    printf("[stub] sip_stop_recording()\n");
    return 0;
}

int sip_list_audio_devices(
    sip_audio_direction_t direction,
    sip_audio_device_t *devices,
    uint32_t max_devices)
{
    (void)direction;
    if (devices == NULL) {
        return 0; /* no devices */
    }
    (void)max_devices;
    return 0;
}

int sip_select_audio_device(
    sip_audio_direction_t direction,
    const char *device_id)
{
    (void)direction; (void)device_id;
    return 0;
}

int sip_get_audio_device(
    sip_audio_direction_t direction,
    sip_audio_device_t *device)
{
    (void)direction;
    if (device) {
        device->id = "default";
        device->name = "Default Device";
        device->direction = SIP_AUDIO_INPUT;
        device->available = true;
    }
    return 0;
}

int sip_send_heartbeat(void)
{
    return 0;
}

int sip_get_health(sip_health_t *health)
{
    if (health) {
        health->status = SIP_STATUS_REGISTERED;
        health->internet_connected = true;
        health->registered = true;
        health->recording = false;
        health->heartbeat_age_ms = 0;
        health->recording_bytes = 0;
        health->last_error = 0;
    }
    return 0;
}

void sip_shutdown(void)
{
}
