# Custome_development

## SIP communication

`sip_communication.h` defines the interface for registering a SIP line,
monitoring internet and heartbeat health, and triggering outbound SIP calls.
Provide the static SIP number, password, SIP server, and network values from a
private deployment configuration before calling `sip_init()`. Real passwords
must not be committed to this repository.

Recording is opt-in. Set `SIP_RECORDING_ENABLED` to `1`, configure
`SIP_RECORDING_FOLDER`, and call `sip_start_recording()` for an active call.
The folder must already exist and be writable. Applications must obtain any
consent required by local law before recording SIP communications.

Audio devices can be selected dynamically. Call `sip_list_audio_devices()` to
discover available microphones and speakers, then pass the selected device ID
to `sip_select_audio_device()`. The active microphone and speaker can be
changed during a call; `"default"` uses the operating system default device.

## One-button example

`main.c` shows the complete call flow. Set `ONE_BUTTON_DESTINATION` to the
destination SIP URI, then connect `one_button_call()` to the physical button
or UI event in the target application. The sample console loop uses `c` as
the button action and `q` to exit. Link the file with the implementation of
the functions declared in `sip_communication.h`.

## PJSIP/MicroSIP backend

The implementation follows the PJSUA integration points used by the attached
MicroSIP 3.22.3 source: PJSUA account registration and call creation, audio
device enumeration and switching, and conference-bridge recording. Define
`SIP_USE_PJSUA=1` and link the PJSIP/PJMEDIA libraries from the MicroSIP build
environment to enable this backend. Without that define, unsupported operations
return `SIP_ERROR_UNSUPPORTED`; no fake call is reported as successful.

## Enhanced features

The SIP communication library includes:

- **Call state tracking**: Automatic state transitions on calling, early media,
  confirmed, and disconnected events.
- **Registration callbacks**: Real-time SIP registration status with detailed
  HTTP response codes.
- **Recording file management**: Automatic filename generation using ISO 8601
  timestamps; recordings are closed immediately on call disconnect.
- **Heartbeat monitoring**: Duration tracking for active calls and recording
  sessions with estimated bytes recorded.
- **Comprehensive logging**: Debug output on stderr for initialization, calls,
  recording, device selection, and shutdown.
- **Error reporting**: Specific error codes for invalid arguments, uninitialized
  state, backend failures, and unsupported operations.
