# VitaAssistant

A Home Assistant client for the PS Vita. Control your lights, climate, scenes, switches, and media players directly from your Vita.

## Features

- Browse entities by category: Home, Lights, Climate, Scenes, Devices
- Toggle lights and switches
- Adjust light brightness and color (RGB/HS)
- Set thermostat temperature
- Activate scenes
- Fire TV remote control overlay
- Touchscreen and button/analog stick input

## Requirements

- A PS Vita with HENkaku/Enso
- [VitaSDK](https://vitasdk.org/) installed
- A Home Assistant instance on your local network
- A Home Assistant [long-lived access token](https://developers.home-assistant.io/docs/auth_api/#long-lived-access-tokens)

## Setup

1. Clone the repo:
   ```
   git clone https://github.com/yourusername/VitaAssistant.git
   cd VitaAssistant
   ```

2. Copy the example config and fill in your details:
   ```
   cp src/config_secret.h.example src/config_secret.h
   ```
   Edit `src/config_secret.h` with your Home Assistant IP, port, access token, and entity IDs.

3. Build:
   ```
   mkdir build && cd build
   cmake ..
   make
   ```

4. Install the generated `VitaAssistant.vpk` on your Vita via VitaShell.

5. Make sure your Vita is connected to the same WiFi network as your Home Assistant instance.

## Controls

| Input | Action |
|-------|--------|
| L/R Triggers | Switch tabs |
| D-pad Up/Down | Navigate entities |
| D-pad Left/Right | Adjust brightness / temperature |
| Left Stick | Fine brightness adjust |
| X | Toggle / Activate |
| Triangle | Color picker (RGB lights) / Remote (media players) |
| Circle | Cycle sub-selection on lights |
| Select | Manual refresh |
| Start | Exit |
| Touch | Tap tabs or entities |

## License

MIT
