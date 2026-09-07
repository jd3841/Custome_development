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
