This program tries to replicate the functionality of [X52LuaOut](https://forums.x-plane.org/index.php?%2Ffiles%2Ffile%2F35304-x52luaout-winmaclin) with Microsoft Flight Simulator 2020. If you are proficient in Lua or C++, feel free to help out and send PRs. I’m not experienced in any of them, nor have much time, so my progress is slow.

The program can re-use the XML configuration files from X52LuaOut, but you have to replace the name of the datarefs with their MSFS equivalents called SimVars. See `default.xml` included with x52msfsout for examples.

The program is NOT user-friendly. I only developed the bare minimum functionality that I needed. You are welcomed to help out.

I have an X52 Pro, so I cannot support the non-pro version because I cannot test it.

X52LuaOut supports Windows, Linux, and Mac. This program currently only supports Windows. I know that MSFS can be started under Linux in Steam using Proton but I don’t know whether the SimConnect SDK works in Linux. I have zero information about Mac.

# Installation

Download [X52LuaOut](https://forums.x-plane.org/index.php?%2Ffiles%2Ffile%2F35304-x52luaout-winmaclin) and extract it somewhere. If you have X-Plane and already installed X52LuaOut, you can reuse it, as well.

Follow the install instructions of X52LuaOut for Windows (see PDF among the extracted files). Particularly, I think you will need to do the libusb installation. Please report your experience by creating an Issue at the top of this page. I would like to include your experience in this documentation.

Download the latest x52msfsout Release from the right hand side of this page and copy the .exe file into `the folder where you extracted X52LuaOut.zip/x52LuaOut/internals` or `the folder of X-Plane.exe\Resources\plugins\FlyWithLua\Scripts\x52LuaOut\internals\`.

# XML configuration files

After installation, you have to prepare XML configuration file(s). They determine how your joystick will react to events in MSFS  (including shifted buttons and Mode 1-3). You can define your own MFD pages, LED states with custom blinking patterns, etc. The included `default.xml` file has example configurations for all tags which are currently supported.

x52msfsout only validates whether your file is well-formed XML, but it does not check whether mandatory attributes are missing and things like that. So it is your responsibility to create the XML according to the X52LuaOut Manual, otherwise x52msfsout will likely not work or will crash.

To create these files, your primary documentation is the X52LuaOut Manual (you downloaded it during installation). But x52msfsout introduced some changes which are described below.

## X52LuaOut configuration file support

In the included `default.xml` file you can find example configuration for all tags which are currently supported:

- [x] \<master\> fully supported, with \<target id=mfd\> and \<target id=led\>.

- [x] \<shift_states\> fully supported.

- [ ] \<assignments\>, clear_all attribute not supported. Not all types of \<button\> tags are supported inside \<assignments\>:
  - [x] \<button type="trigger_pos"\> fully supported. Can handle dataref (SimVar in MSFS), and command (inputevent in MSFS).
  - [x] \<shifted_button\> fully supported with dataref and command attributes.
  - [ ] \<button type="trigger_neg"\>
  - [ ] \<button type="hold"\>
  - [ ] \<button type="toggle"\>
  - [ ] \<button type="repeater"\>

## Differences between X-Plane datarefs and MSFS SimVars

- SimVar names do not look like folder paths (no / signs).
- They are not case sensitive, they can be written in any case.
- They can have spaces.

## SimVar syntax

You use SimVar names in the dataref attributes.

- Some of them have an index, for example, to identify throttle 1 and throttle 2. Add the index to the end of the name separated by a colon like this: `LIGHT GLARESHIELD:1`.
- Every SimVar must have a unit (of measurement)! Add this at the very end of the name separated by a percentage sign like this: `LIGHT GLARESHIELD:1%number`. The default unit for each SimVar can be found in the [official SimVar documentation](https://docs.flightsimulator.com/html/Programming_Tools/SimVars/Simulation_Variables.htm). Also see the [docs on Simulation Variable Units](https://docs.flightsimulator.com/html/Programming_Tools/SimVars/Simulation_Variable_Units.htm) to see how SimVars can be converted to different units. You have to take this into account when putting together your XML because the same SimVar can return different values depending on what unit you specify. Perferably use [SimvarWatcher-MFWASM](https://github.com/rmaryan/SimvarWatcher-MFWASM) first to test what min and max values MSFS returns for a certain unit.

## Where can I find SimVar names?

- Look it up in the [official SimVar documentation](https://docs.flightsimulator.com/html/Programming_Tools/SimVars/Simulation_Variables.htm).
- Enable Development mode in MSFS in Options → General Options → Developers. During a flight, from the top Devmode menu select Tools → Behaviors. On the LocalVariables tab you can monitor many SimVars and see when they change and to what value. Optionally, you can check „Show only vars set by plane” to only monitor values defined in the current aircraft. Then use these names in the XML.
- Use [SimvarWatcher-MFWASM](https://github.com/rmaryan/SimvarWatcher-MFWASM) to test SimVar values.

## What are InputEvents?

You use InputEvents in command attributes. They command the plane to do something.

To find available InputEvents for a given aircraft, enable Development mode in MSFS in Options → General Options → Developers. During a flight, from the top Devmode menu select Tools → Behaviors. At the top of the page select AIRCRAFTNAME_INTERIOR.XML (for example CESSNA152_INTERIOR.XML) from the dropdown. Then on the InputEvents tab you can drill down into several event groups. At the lowest level everything written in light blue are InputEvent names, like ELEV_TRIM_UP or ELEVATOR_TRIM_SET. Find the documentation of the Behaviors tool [on this page](https://docs.flightsimulator.com/html/Developer_Mode/Menus/Tools/Behaviors_Debug.htm).

## Joystick button numbers in MSFS

X52 Pro has 39 buttons. MSFS recognizes all buttons but SimConnect gives an error for buttons above 32. This bug has been reported [on the developer forum](https://devsupport.flightsimulator.com/t/7708). So currently you cannot use buttons above 32 in x52msfsout.

When specifying the button numbers for the \<button\> tag and elsewhere, use the same button number that you see in MSFS Control Options.

Find an easy to understand overview of different buttons [on this page](./X52 Pro Buttons Overview/X52 Pro Buttons Overview.md)

# Usage

After you have completed the installation steps and you have an XML configuration file ready, do the following.

Plug in your X52 Pro.

Open Windows „USB game controllers” panel then open X52 Professional H.O.T.A.S. properties. Special buttons, like the Mode wheel only work for me (both in X-Plane and MSFS) if I keep the properties open while x52msfsout is running. Please report in an Issue whether you need this or not. I’m not sure why I need it.

Start MSFS.

Start your flight.

Start x52msfsout.exe. This will start the command handler from X52LuaOut which handles the communication with the joystick. Make sure you start x52msfsout only now, after you have started your flight. Otherwise the program will not be able to detect the current position of your Mode wheel. However, if this happens you can still turn the wheel to a different mode to make the program start recognizing it. After that it will work fine.

x52msfsout has command line options:

- `help` displays all possible options
- `xmlconfig` Mandatory! Tells the program which XML config file to use (you can have different files for different aircrafts). The XML files are not opened automatically based on the aircraft’s name like in X52LuaOut.
- `joystick` is an integer number which is the ID of your X52 joystick in MSFS. MSFS does not provide a way to find out your joystick’s ID automatically. This number can change depending on how many peripherals you have connected. Start with 0 and then go up from there until your button presses are registered in `x52msfsout_log.txt`.


## Development environment

The project was created using Visual Studio 2022.

To compile it yourself, you will need the SimConnect SDK, the [fmt lib](https://github.com/fmtlib/fmt/), and the Boost headers. If you need help setting up your dev environment, open an Issue and I can hopefully help.

## Architecture

This program is a rewrite of only the X52LuaOut script for FlyWithLua. But X52LuaOut has an other part, the command handler. x52msfsout simply reuses this part. Here is how everything connects together:

![](Architecture.svg)

# Known bugs

- Only 32 joystick buttons are supported instead of 39. Bug reported.
- The MFD Shift indicator is constantly on.
- At some exit statements the cmd_handler is not closed.
- Nice to have: MapClientEventToSimEvent does not allow re-use of Event IDs. Workaround already implemented. Bug reported.

# Online presence

This program has been announced at https://forums.flightsimulator.com/t/controlling-leds-and-mfd-on-the-saitek-logitech-x52-pro-and-non-pro-joystick/223066

and at https://forums.x-plane.org/index.php?%2Ffiles%2Ffile%2F35304-x52luaout-winmaclin

# TODO

Announce this program at https://flightsim.to/file/5559/saitek-x52-pro-dynamic-led-plugin