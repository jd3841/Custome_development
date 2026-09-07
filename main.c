#include "sip_communication.h"

#include <stdio.h>

/*
 * Replace this with the SIP URI assigned to the one-button destination.
 * Keep credentials and network settings in a private build configuration.
 */
#ifndef ONE_BUTTON_DESTINATION
#define ONE_BUTTON_DESTINATION "sip:1002@example.com"
#endif

/*
 * Platform-specific code should call this function when the physical or UI
 * one-button action is pressed.
 */
static int one_button_call(void)
{
    int result = sip_trigger_call(ONE_BUTTON_DESTINATION);

    if (result < 0) {
        fprintf(stderr, "Unable to start SIP call (error %d)\n", result);
        return result;
    }

    printf("SIP call started: %s\n", ONE_BUTTON_DESTINATION);

    /*
     * Recording is opt-in. The SIP implementation will reject this when
     * recording is disabled in sip_default_config.
     */
    if (sip_default_config.recording_enabled) {
        result = sip_start_recording(sip_default_config.recording_folder);
        if (result < 0) {
            fprintf(stderr, "Unable to start call recording (error %d)\n", result);
        }
    }

    return 0;
}

/*
 * Replace this loop with the target platform's event loop, GPIO handler, or
 * GUI callback. The example uses 'c' as the one-button action and 'q' to exit.
 */
int main(void)
{
    sip_health_t health;
    int result = sip_init(&sip_default_config);

    if (result < 0) {
        fprintf(stderr, "SIP initialization failed (error %d)\n", result);
        return 1;
    }

    result = sip_connect();
    if (result < 0) {
        fprintf(stderr, "SIP connection failed (error %d)\n", result);
        sip_shutdown();
        return 1;
    }

    printf("SIP ready. Press 'c' to call or 'q' to quit.\n");

    for (;;) {
        int command = getchar();

        if (command == 'c' || command == 'C') {
            (void)one_button_call();
        } else if (command == 'q' || command == 'Q' || command == EOF) {
            break;
        }

        /*
         * In an embedded or GUI application, call this from a periodic
         * scheduler instead of blocking on getchar().
         */
        result = sip_send_heartbeat();
        if (result < 0) {
            fprintf(stderr, "SIP heartbeat failed (error %d)\n", result);
        }

        result = sip_get_health(&health);
        if (result == 0 && health.status == SIP_STATUS_ERROR) {
            fprintf(stderr, "SIP communication error (%d)\n", health.last_error);
        }
    }

    (void)sip_stop_recording();
    sip_shutdown();
    return 0;
}
