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
- Auto-refresh every ~3 seconds — changes made elsewhere (phone, dashboard) show up on the Vita shortly after

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

### Main view

| Input | Action |
|-------|--------|
| L/R Triggers | Switch tabs |
| D-pad Up/Down | Navigate entities |
| D-pad Left/Right | Adjust brightness (lights) / target temperature (climate) |
| Left Stick Left/Right | Fine brightness adjust on a light |
| X | Toggle light/switch, activate scene, play/pause media |
| Triangle | Color picker (RGB lights) / Fire TV remote (media players) |
| Circle | Cycle sub-selection on lights (card → toggle → slider) |
| Select | Manual refresh |
| Start | Exit |
| Touch | Tap a tab to switch; tap the right side of a card to toggle/activate |

### Color picker overlay (Triangle on an RGB light)

| Input | Action |
|-------|--------|
| D-pad Up/Down | Switch between Hue / Saturation / Brightness |
| D-pad Left/Right | Coarse adjust |
| Left Stick Left/Right | Fine adjust |
| X | Apply color to the light |
| Circle or Triangle | Close overlay |

### Fire TV remote overlay (Triangle on a `media_player`)

| Input | Action |
|-------|--------|
| D-pad | Navigate button grid |
| Left Stick | Send directional commands to Fire TV |
| X | Press the highlighted button |
| Square | OK / Select shortcut |
| L Trigger | Volume down (hold to repeat) |
| R Trigger | Volume up (hold to repeat) |
| Circle | Back |
| Triangle | Close overlay |

## Troubleshooting

The status line in the top-right of the header shows what the app is doing. Common messages:

| Message | Meaning |
|---------|---------|
| `HTTP 401 — check token in config` | Token is missing, wrong, or has been revoked. Regenerate in HA and update `config.txt`. |
| `HTTP 403 — check token in config` | Token is valid but doesn't have permission. Make sure it's a long-lived access token under your user. |
| `HTTP 404 …` | `host`/`port` is reachable but the `/api/` path isn't — most often a reverse-proxy config that doesn't forward the API. |
| `HTTP 5xx …` | Home Assistant itself is erroring — check HA's log. |
| `Network error — check WiFi/host` | Couldn't reach HA at all — wrong IP, Vita not on WiFi, or HA down. Press **Select** to retry. |
| `Config required` (full-screen) | `config.txt` is missing or invalid. A template is created automatically on first launch at `ux0:data/VitaAssistant/config.txt`. |

> Auto-refresh pauses while you're actively adjusting a brightness slider, so your in-progress value won't snap back mid-dim.

## License

MIT
