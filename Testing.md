# Test

- Plug in X52Pro
- Start the "X52 Professional H.O.T.A.S." application. On the Programming tab, send out the provided "x52LuaOut clear.pr0" file so that the throttle scroll wheel will not work as zoom in MSFS.
- Open services.msc and scroll down to the "Logitech DirectOutput" service. Make sure it is running.
- Start MSFS
- cd into the directory where x52msfsout.exe is.
- Start x52msfsout with default_invalid.xml in this repository. It should quit with an error.
- Load a flight with C152 at a parking spot.
- Open Devmode -> Tools -> SimConnect Inspector.
- Start x52msfsout.
- In services.msc, refresh the window and check that the "Logitech DirectOutput" service has been stopped.
- Battery Master switch ON (the right side one of the two red switches). LEDs and MFD should stay off, because currently their brightness is zero.
- Dome light should change the brightness of the MFD, the Mode LED, the pov LED, and the blinking d LED.
- LED's brightness should be 50% when dome light is at maximum.
- When Battery Master switch is switched off, MFD and LEDs should switch off and MFD texts should be cleared.
- Battery Master switch ON
- Pressing Pinkie button should write ‘mode1_shiftStick’ into x52msfsout_log.txt.

- Moving the Mode Wheel to 2 should write "Shift state was cleared." and "Joy Button 29 was pressed" into logfile. Also, the SHIFT indicator on the MFD should switch off.
- Moving the Mode Wheel to 3 should write New shift state: mode3 into logfile, and the SHIFT indicator on the MFD should switch on.
- Switch back to Mode 1.
- Throttle scrollwheel down should move elevator trim nose down. This is done using an InputEvent.
- Throttle scrollwheel up should move elevator trim nose up. This is done using an InputEvent.
- Throttle scrollwheel up (or down) in Mode 1 with Pinkie button should move elevator trim nose up (or down) in big increments. This is done using calculator code.
- Throttle scrollwheel press should reset elevator trim to middle position. This is done by setting a SimVar to a value.
- Led D on the throttle should be flashing red twice quickly, because parking brake is set.
- Disengaging parking brake with the mouse should flash Led D 4 times in amber, then flashing should stop.
- Setting the flaps from 0 to 10 should set the T1 led to green. Flaps 20 should set it to yellow and 30 to red. This tests that leds can be successfully set to a constant color.
- Throttle scrollwheel press in Mode 1 with Pinkie shift should toggle parking brakes on. This is done using an InputEvent.
- SimConnect Inspector must show no exceptions for "x52 msfs out client", except
  - one ClearDataDefinition EXCEPTION_UNRECOGNIZED_ID
  - on the command line one MyDispatchProcRD Received unhandled SIMCONNECT_RECV ID:2
- Quit x52msfsout by q+Enter.
- In services.msc, refresh the window and check that the "Logitech DirectOutput" service is running again.

