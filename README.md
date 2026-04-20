# VitaAssistant

[![Build VPK](https://github.com/RobDeGeorge/VitaAssistant/actions/workflows/build.yml/badge.svg)](https://github.com/RobDeGeorge/VitaAssistant/actions/workflows/build.yml)
[![Latest Release](https://img.shields.io/github/v/release/RobDeGeorge/VitaAssistant?include_prereleases&sort=semver)](https://github.com/RobDeGeorge/VitaAssistant/releases/latest)

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

- A PS Vita with HENkaku/Enso and [VitaShell](https://github.com/TheOfficialFloW/VitaShell)
- A Home Assistant instance on your local network
- A Home Assistant [long-lived access token](https://developers.home-assistant.io/docs/auth_api/#long-lived-access-tokens)

## Install

1. Download the latest `VitaAssistant.vpk` from the [Releases page](https://github.com/RobDeGeorge/VitaAssistant/releases).
2. Transfer it to your Vita (VitaShell FTP, USB, or SD card).
3. In VitaShell, navigate to the `.vpk` and press **X** to install.
4. Launch VitaAssistant once.
   - It will create a template config at `ux0:data/VitaAssistant/config.txt` and show an instruction screen. Press **Start** to exit.
5. Edit `ux0:data/VitaAssistant/config.txt` with your Home Assistant details (see [Configuration](#configuration)).
6. Relaunch VitaAssistant. It should connect and populate the entity list.

> The VPK is identical for everyone — no rebuild needed per user. All per-user settings live in the config file.

## Configuration

The config file lives at `ux0:data/VitaAssistant/config.txt` and uses simple `key=value` lines. Lines starting with `#` are ignored.

```
host=192.168.1.100
port=8123
token=paste-your-long-lived-token-here

# Optional — only needed if you want the Fire TV remote overlay
fire_tv_media=media_player.your_fire_tv
fire_tv_remote=remote.your_fire_tv
```

| Key | Required | Description |
|-----|----------|-------------|
| `host` | yes | Home Assistant IP address or hostname on your LAN |
| `port` | no (default `8123`) | Home Assistant port |
| `token` | yes | Long-lived access token (Profile → Security in HA) |
| `fire_tv_media` | no | `media_player.*` entity ID for the Fire TV remote overlay |
| `fire_tv_remote` | no | `remote.*` entity ID for the Fire TV remote overlay |

### Editing on the Vita

VitaShell has a built-in text editor — no PC needed after install:

1. Open VitaShell, navigate to `ux0:data/VitaAssistant/config.txt`.
2. Press **Triangle** → **Open decrypted**.
3. Edit, press **Start** to save, and relaunch VitaAssistant.

### Editing from a PC

Copy the file off the Vita via FTP or USB, edit in any text editor, copy it back.

## Building from source

Only needed if you want to modify VitaAssistant itself — regular users just download the prebuilt VPK above.

Requires [VitaSDK](https://vitasdk.org/).

```
git clone https://github.com/RobDeGeorge/VitaAssistant.git
cd VitaAssistant
cmake -S . -B build
cmake --build build
```

The `.vpk` will be at `build/VitaAssistant.vpk`.

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
