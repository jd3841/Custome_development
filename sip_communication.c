#include "sip_communication.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if SIP_USE_PJSUA
#include <pjsua-lib/pjsua.h>
#endif

static const sip_config_t *g_config;
static sip_health_t g_health;
static char g_input_device[32] = "default";
static char g_output_device[32] = "default";
static uint64_t g_recording_start_time;
static uint64_t g_call_start_time;
static uint64_t g_last_heartbeat_time;

static void set_error(int error);

#if SIP_USE_PJSUA
static pjsua_acc_id g_account = PJSUA_INVALID_ID;
static pjsua_call_id g_call = PJSUA_INVALID_ID;
static pjsua_recorder_id g_recorder = PJSUA_INVALID_ID;
static int g_pjsua_started;

static void on_registration_state(
    pjsua_acc_id account_id,
    pjsua_reg_info *registration)
{
    (void)account_id;
    if (registration && registration->cbparam) {
        if (registration->cbparam->code >= 200 &&
            registration->cbparam->code < 300) {
            g_health.status = SIP_STATUS_REGISTERED;
            g_health.internet_connected = true;
            g_health.registered = true;
            g_health.last_error = 0;
            fprintf(stderr, "[SIP] Registration successful (code %d)\n",
                registration->cbparam->code);
        } else {
            g_health.registered = false;
            g_health.internet_connected = false;
            set_error(SIP_ERROR_BACKEND);
            fprintf(stderr, "[SIP] Registration failed (code %d)\n",
                registration->cbparam->code);
        }
    }
}

static void on_call_state(pjsua_call_id call_id, pjsip_event *event)
{
    pjsua_call_info call_info;
    
    (void)event;
    
    if (pjsua_call_get_info(call_id, &call_info) != PJ_SUCCESS) {
        return;
    }
    
    fprintf(stderr, "[SIP] Call state changed: %s (state=%d, media=%d)\n",
        call_info.state_text.ptr, call_info.state, call_info.media_status);
    
    switch (call_info.state) {
        case PJSIP_INV_STATE_CALLING:
            g_health.status = SIP_STATUS_CALLING;
            g_call_start_time = (uint64_t)time(NULL);
            break;
        case PJSIP_INV_STATE_EARLY:
            g_health.status = SIP_STATUS_CALLING;
            break;
        case PJSIP_INV_STATE_CONFIRMED:
            g_health.status = SIP_STATUS_IN_CALL;
            g_call_start_time = (uint64_t)time(NULL);
            break;
        case PJSIP_INV_STATE_DISCONNECTED:
            if (g_recorder != PJSUA_INVALID_ID) {
                sip_stop_recording();
            }
            g_health.status = SIP_STATUS_REGISTERED;
            g_call = PJSUA_INVALID_ID;
            break;
        default:
            break;
    }
}

static uint32_t now_ms(void)
{
    return (uint32_t)((uint64_t)clock() * 1000U / (uint64_t)CLOCKS_PER_SEC);
}
#endif

static void set_error(int error)
{
    g_health.status = SIP_STATUS_ERROR;
    g_health.last_error = error;
}

#if SIP_USE_PJSUA
static int parse_device_id(const char *device_id)
{
    char *end;
    long value;

    if (!device_id || strcmp(device_id, "default") == 0) {
        return PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
    }
    value = strtol(device_id, &end, 10);
    if (*device_id == '\0' || *end != '\0' || value < 0) {
        return -1;
    }
    return (int)value;
}

static int apply_audio_devices(void)
{
    pjsua_snd_dev_param params;
    int input = parse_device_id(g_input_device);
    int output = parse_device_id(g_output_device);

    if (input < 0 || output < 0) {
        return SIP_ERROR_INVALID_ARGUMENT;
    }
    pjsua_snd_dev_param_default(&params);
    params.capture_dev = input;
    params.playback_dev = output;
    return pjsua_set_snd_dev2(&params) == PJ_SUCCESS
        ? SIP_OK : SIP_ERROR_BACKEND;
}
#endif

int sip_init(const sip_config_t *config)
{
    if (!config || !config->sip_number || !config->sip_password ||
        !config->sip_server) {
        return SIP_ERROR_INVALID_ARGUMENT;
    }

    memset(&g_health, 0, sizeof(g_health));
    g_config = config;
    g_health.status = SIP_STATUS_OFFLINE;
    strncpy(g_input_device, config->microphone_device_id
        ? config->microphone_device_id : "default", sizeof(g_input_device) - 1);
    strncpy(g_output_device, config->speaker_device_id
        ? config->speaker_device_id : "default", sizeof(g_output_device) - 1);
    g_input_device[sizeof(g_input_device) - 1] = '\0';
    g_output_device[sizeof(g_output_device) - 1] = '\0';

#if SIP_USE_PJSUA
    {
        pjsua_config ua_config;
        pjsua_logging_config logging_config;
        pjsua_media_config media_config;
        pjsua_transport_config transport_config;
        pjsua_acc_config account_config;
        pjsua_transport_id transport_id;
        pj_status_t status;
        char identity[256];
        char registrar[256];

        pjsua_config_default(&ua_config);
        ua_config.cb.on_reg_state2 = &on_registration_state;
            ua_config.cb.on_call_state = &on_call_state;
            ua_config.thread_cnt = 1;
            ua_config.nameserver_count = 1;
            if (config->dns_server && config->dns_server[0] != '\0') {
                ua_config.nameserver[0] = pj_str((char *)config->dns_server);
            }
        pjsua_media_config_default(&media_config);
        status = pjsua_create();
        if (status != PJ_SUCCESS) {
            set_error(SIP_ERROR_BACKEND);
            return SIP_ERROR_BACKEND;
        }
        status = pjsua_init(&ua_config, &logging_config, &media_config);
        if (status != PJ_SUCCESS) {
            pjsua_destroy();
            set_error(SIP_ERROR_BACKEND);
            return SIP_ERROR_BACKEND;
        }
        pjsua_transport_config_default(&transport_config);
        transport_config.port = config->sip_port;
        status = pjsua_transport_create(PJSIP_TRANSPORT_UDP,
            &transport_config, &transport_id);
        if (status != PJ_SUCCESS) {
            pjsua_destroy();
            set_error(SIP_ERROR_BACKEND);
            return SIP_ERROR_BACKEND;
        }
        status = pjsua_start();
        if (status != PJ_SUCCESS) {
            pjsua_destroy();
            set_error(SIP_ERROR_BACKEND);
            return SIP_ERROR_BACKEND;
        }
        g_pjsua_started = 1;
        snprintf(identity, sizeof(identity), "sip:%s@%s",
            config->sip_number, config->sip_server);
        snprintf(registrar, sizeof(registrar), "sip:%s:%u",
            config->sip_server, (unsigned)config->sip_port);
        pjsua_acc_config_default(&account_config);
        account_config.id = pj_str(identity);
        account_config.reg_uri = pj_str(registrar);
        account_config.cred_count = 1;
        account_config.cred_info[0].realm = pj_str("*");
        account_config.cred_info[0].scheme = pj_str("digest");
        account_config.cred_info[0].username = pj_str((char *)config->sip_number);
        account_config.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
        account_config.cred_info[0].data = pj_str((char *)config->sip_password);
        status = pjsua_acc_add(&account_config, PJ_TRUE, &g_account);
        if (status != PJ_SUCCESS) {
            pjsua_destroy();
            g_pjsua_started = 0;
            set_error(SIP_ERROR_BACKEND);
            return SIP_ERROR_BACKEND;
        }
        apply_audio_devices();
    }
#endif
    return SIP_OK;
}

int sip_connect(void)
{
    if (!g_config) {
        fprintf(stderr, "[SIP] Not initialized\n");
        return SIP_ERROR_NOT_INITIALIZED;
    }
#if SIP_USE_PJSUA
    if (!g_pjsua_started || g_account == PJSUA_INVALID_ID) {
        fprintf(stderr, "[SIP] PJSUA not started or account not registered\n");
        return SIP_ERROR_NOT_CONNECTED;
    }
    fprintf(stderr, "[SIP] Waiting for registration...\n");
#endif
    g_health.status = SIP_STATUS_CONNECTING;
    g_health.internet_connected = true;
    g_health.last_error = 0;
    return SIP_OK;
}

int sip_trigger_call(const char *destination_sip_uri)
{
    if (!g_config || !destination_sip_uri || destination_sip_uri[0] == '\0') {
        fprintf(stderr, "[SIP] Invalid trigger_call parameters\n");
        return SIP_ERROR_INVALID_ARGUMENT;
    }
    
    fprintf(stderr, "[SIP] Triggering call to: %s\n", destination_sip_uri);
    
#if SIP_USE_PJSUA
    {
        pj_str_t uri = pj_str((char *)destination_sip_uri);
        pj_status_t status = pjsua_call_make_call(g_account, &uri, NULL,
            NULL, NULL, &g_call);
        if (status != PJ_SUCCESS) {
            fprintf(stderr, "[SIP] Call failed with status: %d\n", status);
            set_error(SIP_ERROR_BACKEND);
            return SIP_ERROR_BACKEND;
        }
        fprintf(stderr, "[SIP] Call initiated successfully\n");
    }
#else
    (void)destination_sip_uri;
    fprintf(stderr, "[SIP] PJSUA not enabled, call not supported\n");
    set_error(SIP_ERROR_UNSUPPORTED);
    return SIP_ERROR_UNSUPPORTED;
#endif
    g_health.status = SIP_STATUS_CALLING;
    return SIP_OK;
}

int sip_start_recording(const char *recording_folder)
{
    if (!g_config || !recording_folder || recording_folder[0] == '\0') {
        return SIP_ERROR_INVALID_ARGUMENT;
    }
    if (!g_config->recording_enabled) {
        return SIP_ERROR_UNSUPPORTED;
    }
#if SIP_USE_PJSUA
    {
        char filename[512];
        pj_str_t path;
        pjsua_conf_port_id recorder_port;
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        
        if (g_call == PJSUA_INVALID_ID) {
            return SIP_ERROR_NOT_CONNECTED;
        }
        snprintf(filename, sizeof(filename),
            "%s/sip_call_%04d%02d%02d_%02d%02d%02d.wav",
            recording_folder,
            timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1,
            timeinfo->tm_mday,
            timeinfo->tm_hour,
            timeinfo->tm_min,
            timeinfo->tm_sec);
        
        path = pj_str(filename);
        if (pjsua_recorder_create(&path, 0, NULL, -1, 0, &g_recorder)
            != PJ_SUCCESS) {
            fprintf(stderr, "[SIP] Failed to create recorder at %s\n",
                filename);
            set_error(SIP_ERROR_BACKEND);
            return SIP_ERROR_BACKEND;
        }
        
        recorder_port = pjsua_recorder_get_conf_port(g_recorder);
        if (pjsua_conf_connect(pjsua_call_get_conf_port(g_call),
            recorder_port) != PJ_SUCCESS) {
            fprintf(stderr, "[SIP] Failed to connect call to recorder\n");
            pjsua_recorder_destroy(g_recorder);
            g_recorder = PJSUA_INVALID_ID;
            set_error(SIP_ERROR_BACKEND);
            return SIP_ERROR_BACKEND;
        }
        pjsua_conf_connect(0, recorder_port);
        
        g_recording_start_time = (uint64_t)now;
        fprintf(stderr, "[SIP] Recording started: %s\n", filename);
    }
#else
    (void)recording_folder;
    set_error(SIP_ERROR_UNSUPPORTED);
    return SIP_ERROR_UNSUPPORTED;
#endif
    g_health.recording = true;
    g_health.recording_bytes = 0;
    return SIP_OK;
}

int sip_stop_recording(void)
{
#if SIP_USE_PJSUA
    if (g_recorder != PJSUA_INVALID_ID) {
        pjsua_recorder_destroy(g_recorder);
        g_recorder = PJSUA_INVALID_ID;
    }
#else
    if (!g_health.recording) {
        return SIP_OK;
    }
    return SIP_ERROR_UNSUPPORTED;
#endif
    g_health.recording = false;
    return SIP_OK;
}

int sip_list_audio_devices(
    sip_audio_direction_t direction,
    sip_audio_device_t *devices,
    uint32_t max_devices)
{
#if SIP_USE_PJSUA
    pjmedia_aud_dev_info info[PJMEDIA_AUD_MAX_DEVS];
    unsigned count = PJMEDIA_AUD_MAX_DEVS;
    unsigned found = 0;
    unsigned i;
    if (pjsua_enum_aud_devs(info, &count) != PJ_SUCCESS) {
        return SIP_ERROR_BACKEND;
    }
    for (i = 0; i < count; ++i) {
        bool match = direction == SIP_AUDIO_INPUT
            ? info[i].input_count > 0 : info[i].output_count > 0;
        if (match) {
            if (devices && found < max_devices) {
                static char ids[PJMEDIA_AUD_MAX_DEVS][16];
                snprintf(ids[found], sizeof(ids[found]), "%u", i);
                devices[found].id = ids[found];
                devices[found].name = info[i].name;
                devices[found].direction = direction;
                devices[found].available = true;
            }
            ++found;
        }
    }
    return (int)found;
#else
    (void)direction;
    (void)devices;
    (void)max_devices;
    return SIP_ERROR_UNSUPPORTED;
#endif
}

int sip_select_audio_device(
    sip_audio_direction_t direction,
    const char *device_id)
{
    if (!device_id || device_id[0] == '\0') {
        return SIP_ERROR_INVALID_ARGUMENT;
    }
    if (direction == SIP_AUDIO_INPUT) {
        strncpy(g_input_device, device_id, sizeof(g_input_device) - 1);
        g_input_device[sizeof(g_input_device) - 1] = '\0';
    } else if (direction == SIP_AUDIO_OUTPUT) {
        strncpy(g_output_device, device_id, sizeof(g_output_device) - 1);
        g_output_device[sizeof(g_output_device) - 1] = '\0';
    } else {
        return SIP_ERROR_INVALID_ARGUMENT;
    }
#if SIP_USE_PJSUA
    return apply_audio_devices();
#else
    return SIP_ERROR_UNSUPPORTED;
#endif
}

int sip_get_audio_device(
    sip_audio_direction_t direction,
    sip_audio_device_t *device)
{
    if (!device) {
        return SIP_ERROR_INVALID_ARGUMENT;
    }
    if (direction == SIP_AUDIO_INPUT) {
        device->id = g_input_device;
    } else if (direction == SIP_AUDIO_OUTPUT) {
        device->id = g_output_device;
    } else {
        return SIP_ERROR_INVALID_ARGUMENT;
    }
    device->name = device->id;
    device->direction = direction;
    device->available = true;
    return SIP_OK;
}

int sip_send_heartbeat(void)
{
    if (!g_config) {
        return SIP_ERROR_NOT_INITIALIZED;
    }
#if SIP_USE_PJSUA
    uint64_t current_time = (uint64_t)time(NULL);
    
    g_last_heartbeat_time = current_time;
    g_health.heartbeat_age_ms = now_ms() % 1000;
    
    if (g_call != PJSUA_INVALID_ID && g_call_start_time > 0) {
        uint64_t call_duration = current_time - g_call_start_time;
        fprintf(stderr, "[SIP] Heartbeat - Call duration: %llu seconds\n",
            (unsigned long long)call_duration);
    }
    
    if (g_health.recording && g_recording_start_time > 0) {
        uint64_t recording_duration = current_time - g_recording_start_time;
        g_health.recording_bytes = recording_duration * 16000 * 2 / 8;
        fprintf(stderr, "[SIP] Heartbeat - Recording: %llu bytes\n",
            (unsigned long long)g_health.recording_bytes);
    }
#endif
    return SIP_OK;
}

int sip_get_health(sip_health_t *health)
{
    if (!health) {
        return SIP_ERROR_INVALID_ARGUMENT;
    }
    *health = g_health;
    return SIP_OK;
}

void sip_shutdown(void)
{
    fprintf(stderr, "[SIP] Shutdown initiated\n");
    
    sip_stop_recording();
    
#if SIP_USE_PJSUA
    if (g_call != PJSUA_INVALID_ID) {
        fprintf(stderr, "[SIP] Hanging up active call\n");
        pjsua_call_hangup(g_call, 200, NULL, NULL);
        g_call = PJSUA_INVALID_ID;
    }
    
    if (g_pjsua_started) {
        fprintf(stderr, "[SIP] Destroying PJSUA\n");
        pjsua_destroy();
    }
    g_pjsua_started = 0;
    g_account = PJSUA_INVALID_ID;
    g_call = PJSUA_INVALID_ID;
    g_recorder = PJSUA_INVALID_ID;
#endif
    
    g_config = NULL;
    g_call_start_time = 0;
    g_recording_start_time = 0;
    g_last_heartbeat_time = 0;
    memset(&g_health, 0, sizeof(g_health));
    
    fprintf(stderr, "[SIP] Shutdown complete\n");
}
