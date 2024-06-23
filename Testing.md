# Test

- Plug in X52Pro
- Open Windows „USB game controllers” panel and X52 Professional H.O.T.A.S. properties.
- Start MSFS
- Start x52msfsout with an invalid XML. It should quit with an error.
- Load a flight with C152 at a parking spot.
- Open Devmode -> Tools -> SimConnect Inspector.
- Start x52msfsout.
- Battery Master switch ON should switch LEDs on
- Dome light should change MFD brightness
- When Battery Master switch is switched off, MFD and LEDs should switch off and MFD texts should be cleared.
- Pressing Pinkie button should write ‘mode1_shiftStick’ into x52msfsout_log.txt. If it does not happen, make sure you use the correct number for the --joystick parameter.

- Moving the Mode Wheel should write New shift state: mode1, 2, 3 into logfile
- Throttle scrollwheel down should move elevator trim nose down. This is done using an InputEvent.
- Throttle scrollwheel up should move elevator trim nose up. This is done using an InputEvent.
- Throttle scrollwheel press should reset elevator trim to middle position. This is done by setting a SimVar to a value.
- Throttle scrollwheel press in Mode 1 with Pinkie shift should toggle parking brakes on. This is done using an InputEvent.
- SimConnect Inspector must show no exceptions for "x52 msfs out client", except
  - one ClearDataDefinition EXCEPTION_UNRECOGNIZED_ID
  - on the command line one MyDispatchProcRD Received unhandled SIMCONNECT_RECV ID:2
- Is the command handler still running (is it getting heartbeats)?
- Quit x52msfsout by q+Enter. Did the command handler also quit?

