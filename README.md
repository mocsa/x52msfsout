This program tries to replicate the functionality of [X52LuaOut](https://forums.x-plane.org/index.php?%2Ffiles%2Ffile%2F35304-x52luaout-winmaclin) but with Microsoft Flight Simulator 2020. If you are proficient in Lua or C++, feel free to help out and send PRs. I’m not experienced in any of them, nor have much time, so my progress is slow.

The program can re-use the XML configuration files from X52LuaOut, but you have to replace the name of the datarefs with their MSFS equivalents called SimVars. See `default.xml` included with x52msfsout for examples.

The program is NOT user-friendly. I only developed the bare minimum functionality that I needed. You are welcomed to help out.

I have an X52 Pro, so I cannot support the non-pro version because I cannot test it.

X52LuaOut supports Windows, Linux, and Mac. This program currently only supports Windows. I know that MSFS can be started under Linux in Steam using Proton but I don’t know whether the SimConnect SDK works in Linux. I have zero information about Mac.

## XML tags which are handled from X52LuaOut configuration files

- [X] master

- [x] shift_states

- [ ] assignments

# Installation

Download [X52LuaOut](https://forums.x-plane.org/index.php?%2Ffiles%2Ffile%2F35304-x52luaout-winmaclin) and extract it somewhere. If you have X-Plane and already installed X52LuaOut, you can reuse it, as well.

Follow the install instructions of X52LuaOut for Windows (see PDF among the extracted files). Particularly, I think you will need to do the libusb installation.

Download the latest Release from this page and copy the .exe file into `the folder where you extracted X52LuaOut.zip/x52LuaOut/internals` or `the folder of X-Plane.exe\Resources\plugins\FlyWithLua\Scripts\x52LuaOut\internals\`.

Start the .exe. This will start the command handler from X52LuaOut which handles the communication with the joystick.

# Usage

x52msfsout has command line options:

- `help` displays all possible options
- `xmlconfig` Mandatory! Tells the program which XML config file to use (you can have different files for different aircrafts). The XML files are not opened automatically based on the aircraft’s name like in X52LuaOut.
- `joystick` is an integer number which is the ID of your X52 joystick in MSFS. This number can change depending on how many peripherals you have connected. Start with 0 and then go up from there until your button presses are registered in `x52msfsout_log.txt`. MSFS does not provice a way to find out your joystick’s ID automatically.

Make sure you start x52msfsout after you have started your flight. Otherwise the program will not be able to detect the current position of your Mode wheel. However, if this happens you can still turn the wheel to a different mode to make the program start recognizing it. After that it will work fine.

Special buttons, like the Mode wheel only work for me (both in X-Plane and MSFS) if I open the Windows USB Game Controllers window, select „X52 Professional H.O.T.A.S.” then click on Properties to open the properties panel, and keep it open while x52msfsout is running. Please report whether you need this or not. I’m not sure why I need it.

# Architecture

This program is a rewrite of only the X52LuaOut script for FlyWithLua. But X52LuaOut has an other part, the command handler. x52msfsout simply reuses this part. Here is how everything connects together:

![](Architecture.svg)

# XML files

X52LuaOut has basic built-in validation of XML files. x52msfsout only validates whether your file is well-formed XML, but it does not check whether mandatory attributes are missing and things like that. So it is your responsibility to create the XML according to the X52LuaOut documentation, otherwise x52msfsout will likely not work or will crash.

## Differences between X-Plane datarefs and MSFS SimVars

- SimVar names do not look like folder paths (no / signs).
- They are not case sensitive, they can be written in any case.
- They can have spaces.
- Some of them require an index to address, for example, throttle 1 and throttle 2. Add the index to the end of the name separated by a colon like this: `LIGHT GLARESHIELD:1`.
- Every SimVar must have a unit (of measurement)! Add this at the very end of the name separated by a percentage sign like this: `LIGHT GLARESHIELD:1%number`. The default unit for each SimVar can be found in the [official SimVar documentation](https://docs.flightsimulator.com/html/Programming_Tools/SimVars/Simulation_Variables.htm). Also see the [docs on Simulation Variable Units](https://docs.flightsimulator.com/html/Programming_Tools/SimVars/Simulation_Variable_Units.htm) to see how SimVars can be converted to different units. You have to take this into account when putting together your XML because the same SimVar can return different values depending on what unit you specify. Perferably use [SimvarWatcher-MFWASM](https://github.com/rmaryan/SimvarWatcher-MFWASM) first to test what value MSFS returns for a certain unit.

## Where can I find SimVar names

- Look it up in the [official SimVar documentation](https://docs.flightsimulator.com/html/Programming_Tools/SimVars/Simulation_Variables.htm).
- Enable Development mode in MSFS in Options → General Options → Developers. During a flight, from the top Devmode menu select Tools → Behaviors. On the LocalVariables tab you can monitor many SimVars and see when they change and to what value. Optionally, you can check „Show only vars set by plane” to only monitor values defined in the current aircraft. Then use these names in the XML.
- Use [SimvarWatcher-MFWASM](https://github.com/rmaryan/SimvarWatcher-MFWASM) to test SimVar values.

## Joystick buttons in MSFS

X52 Pro has 39 buttons. MSFS recognizes all buttons but SimConnect gives an error for buttons above 32. This bug has been reported [on the developer forum](https://devsupport.flightsimulator.com/t/7708).

When specifying the button numbers for the \<button\> tag and elsewhere, use the same button number that you see in MSFS Control Options.

Find an easy to understand overview of different buttons [on this page](X52 Pro Buttons Overview/X52 Pro Buttons Overview.md)

# Development environment

The project was created using Visual Studio 2022.

To compile it yourself, you will need the SimConnect SDK, the [fmt lib](https://github.com/fmtlib/fmt/), and the Boost headers.

# TODO

Announce this program at https://forums.flightsimulator.com/t/controlling-leds-and-mfd-on-the-saitek-logitech-x52-pro-and-non-pro-joystick/223066

Announce this program at https://forums.x-plane.org/index.php?%2Ffiles%2Ffile%2F35304-x52luaout-winmaclin

Announce this program at https://flightsim.to/file/5559/saitek-x52-pro-dynamic-led-plugin