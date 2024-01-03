# Test

- Plug in X52Pro
- Open Windows „USB game controllers” panel and X52 Professional H.O.T.A.S. properties.
- Start MSFS
- Start x52msfsout with an invalid XML. It should quit with an error.
- Load a flight with C152
- Open Devmode -> SimConnect Inspector.
- Start x52msfsout.
- Battery switch should switch MFD and LEDs on
- Dome light should change MFD and LED brightness
- When Battery switch is switched off, MFD and LEDs should switch off and MFD texts should be cleared.
- Pressing Pinkie button should write ‘mode1_shiftStick’ into logfile

- Moving the Mode Wheel should write mode1, 2, 3 into logfile
- Throttle scrollwheel down should move elevator trim nose down. This is done using an InputEvent.
- Throttle scrollwheel up should move elevator trim nose up. This is done using an InputEvent.
- Throttle scrollwheel press should reset elevator trim to middle position. This is done by setting a SimVar to a value.
- Throttle scrollwheel press in Mode 1 with Pinkie shift should toggle parking brakes on. This is done using an InputEvent.
- SimConnect Inspector must show no exceptions, except
  - one ClearDataDefinition EXCEPTION_UNRECOGNIZED_ID
  - on the command line one MyDispatchProcRD Received unhandled SIMCONNECT_RECV ID:2
- Is the command handler still running (is it getting heartbeats)?
- Quit x52msfsout by q+Enter. Did the command handler also quit?

# Known bugs

- Only 32 joystick buttons are supported instead of 39. Bug reported.
- The MFD Shift indicator is constantly on.
- Nice to have: MapClientEventToSimEvent does not allow re-use of Event IDs. Workaround already implemented. Bug reported.

XML egy helyre tevése

 <!--  <shifted_button shift_state="mode1_shiftStick" calculator_code="(A:ELEVATOR TRIM POSITION, Radians) 0.1 - (&gtA:ELEVATOR TRIM POSITION, Radians)" ></shifted_button>-->