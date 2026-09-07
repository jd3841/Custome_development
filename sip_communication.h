#ifndef SIP_COMMUNICATION_H
#define SIP_COMMUNICATION_H

/*
 * SIP communication interface.
 *
 * The application should provide the SIP credentials and network values at
 * build or deployment time. Do not commit real passwords to source control.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fixed SIP settings used by the communication line.
 *
 * Replace the placeholder values in the application's private configuration
 * before calling sip_init(). The header intentionally contains no secret.
 */
typedef struct {
    const char *sip_number;
    const char *sip_password;
    const char *sip_server;
    uint16_t sip_port;
    const char *local_static_ip;
    const char *local_netmask;
    const char *local_gateway;
    const char *dns_server;
    const char *recording_folder;
    bool recording_enabled;
    const char *microphone_device_id;
    const char *speaker_device_id;
} sip_config_t;

/*
 * Define these macros in the build or private configuration before including
 * this file to use fixed deployment values.
 */
#ifndef SIP_STATIC_NUMBER
#define SIP_STATIC_NUMBER "replace-with-sip-number"
#endif
#ifndef SIP_STATIC_PASSWORD
#define SIP_STATIC_PASSWORD "replace-with-sip-password"
#endif
#ifndef SIP_STATIC_SERVER
#define SIP_STATIC_SERVER "sip.example.com"
#endif
#ifndef SIP_STATIC_IP
#define SIP_STATIC_IP "192.0.2.10"
#endif
#ifndef SIP_STATIC_NETMASK
#define SIP_STATIC_NETMASK "255.255.255.0"
#endif
#ifndef SIP_STATIC_GATEWAY
#define SIP_STATIC_GATEWAY "192.0.2.1"
#endif
#ifndef SIP_STATIC_DNS
#define SIP_STATIC_DNS "192.0.2.53"
#endif
#ifndef SIP_STATIC_PORT
#define SIP_STATIC_PORT 5060
#endif
#ifndef SIP_RECORDING_FOLDER
#define SIP_RECORDING_FOLDER "./sip-recordings"
#endif
#ifndef SIP_RECORDING_ENABLED
#define SIP_RECORDING_ENABLED 0
#endif
#ifndef SIP_MICROPHONE_DEVICE
#define SIP_MICROPHONE_DEVICE "default"
#endif
#ifndef SIP_SPEAKER_DEVICE
#define SIP_SPEAKER_DEVICE "default"
#endif

static const sip_config_t sip_default_config = {
    SIP_STATIC_NUMBER,
    SIP_STATIC_PASSWORD,
    SIP_STATIC_SERVER,
    SIP_STATIC_PORT,
    SIP_STATIC_IP,
    SIP_STATIC_NETMASK,
    SIP_STATIC_GATEWAY,
    SIP_STATIC_DNS,
    SIP_RECORDING_FOLDER,
    SIP_RECORDING_ENABLED,
    SIP_MICROPHONE_DEVICE,
    SIP_SPEAKER_DEVICE
};

typedef enum {
    SIP_AUDIO_INPUT = 0,
    SIP_AUDIO_OUTPUT
} sip_audio_direction_t;

typedef struct {
    const char *id;
    const char *name;
    sip_audio_direction_t direction;
    bool available;
} sip_audio_device_t;

typedef enum {
    SIP_STATUS_OFFLINE = 0,
    SIP_STATUS_CONNECTING,
    SIP_STATUS_REGISTERED,
    SIP_STATUS_CALLING,
    SIP_STATUS_IN_CALL,
    SIP_STATUS_ERROR
} sip_status_t;

typedef struct {
    sip_status_t status;
    bool internet_connected;
    bool registered;
    bool recording;
    uint32_t heartbeat_age_ms;
    uint64_t recording_bytes;
    int last_error;
} sip_health_t;

/**
 * Initialise the SIP line with static credentials and network details.
 *
 * The config pointer must remain valid until sip_shutdown() is called.
 * Returns 0 on success, or a negative error code.
 */
int sip_init(const sip_config_t *config);

/**
 * Register the SIP line and start network communication.
 *
 * Call this after sip_init() and whenever the internet connection is restored.
 */
int sip_connect(void);

/**
 * Trigger an outbound SIP call to the supplied SIP number.
 *
 * The destination must be a complete SIP URI, for example:
 * "sip:1002@example.com".
 */
int sip_trigger_call(const char *destination_sip_uri);

/**
 * Start recording the active SIP communication.
 *
 * The output file is created inside recording_folder using an application
 * generated filename. The folder must exist and be writable.
 * Returns 0 on success, or a negative error code.
 */
int sip_start_recording(const char *recording_folder);

/**
 * Stop the active recording and close its output file.
 */
int sip_stop_recording(void);

/**
 * Enumerate currently available microphones or speakers.
 *
 * Pass NULL for devices to query the number of matching devices without
 * writing an array. Returns the number of devices, or a negative error code.
 */
int sip_list_audio_devices(
    sip_audio_direction_t direction,
    sip_audio_device_t *devices,
    uint32_t max_devices);

/**
 * Select the microphone or speaker used by the active SIP communication.
 *
 * The device_id must come from sip_list_audio_devices(), or be "default".
 * Selection may be changed while a call is active.
 */
int sip_select_audio_device(
    sip_audio_direction_t direction,
    const char *device_id);

/**
 * Read the currently selected microphone or speaker device.
 */
int sip_get_audio_device(
    sip_audio_direction_t direction,
    sip_audio_device_t *device);

/**
 * Send/refresh the SIP heartbeat and update connection health.
 *
 * Call this periodically from the application's scheduler. The interval
 * should be shorter than the heartbeat timeout configured by the SIP server.
 */
int sip_send_heartbeat(void);

/**
 * Read the current registration, internet, and heartbeat state.
 */
int sip_get_health(sip_health_t *health);

/**
 * Stop calls, unregister the SIP line, and release SIP resources.
 */
void sip_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* SIP_COMMUNICATION_H */
