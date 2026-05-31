# NitroShop

A lightweight, responsive C++ directory explorer and downloader client for the Nintendo 3DS (Old 3DS & New 3DS) designed to browse, search, and retrieve files directly to your console's SD card from remote index listings.

## About This Project

NitroShop is very much a **vibecoded** project. It was built through a lot of experimentation, learning, and AI-assisted development. While it may not always follow perfect software engineering practices, the goal is to create a useful and enjoyable experience for the Nintendo DS and 3DS homebrew community.

Community contributions are welcome. If you're interested in helping improve NitroShop, fixing bugs, adding features, or contributing to future updates, feel free to get involved.

## Features

* **Direct Downloads**: Retrieves files from remote directories straight to your console's SD card.
* **Fast Search**: Trigger the 3DS system keyboard to filter the index listing case-insensitively with partial name matches.
* **Alphabetical Fast Navigation**: Press **L** to open an A-Z overlay and jump directly to files starting with any letter.
* **Favorites & Recents**: Keep track of bookmarked files and review your recent download history.
* **Interactive Setup Wizard**: Run a user-friendly wizard on first launch to configure your download folder and credentials.
* **Local Web Configuration Portal**: Enter Archive.org credentials easily via a web page hosted temporarily by the 3DS on your local network.
* **Secure HTTPS Connections**: Leverages standard SSL/TLS certification via a bundled `cacert.pem` root CA bundle.

## Installation

1. Download the latest `NitroShop.3dsx` from the releases page.
2. Copy `NitroShop.3dsx` to the `/3ds/` folder on your SD card.
3. Launch NitroShop using the **Homebrew Launcher**.

## Build Requirements

* devkitPro with devkitARM toolchain
* 3DS libraries (`libctru`, `citro3d`, `citro2d`, `libcurl`)

## How to Build

Open the devkitPro shell, navigate to this directory, and run:

```bash
# Build .3dsx homebrew executable
make
```

## Known Issues

The following issues are currently known and being worked on:

* **Slow download speeds** compared to what some users may expect.

## To-Do

Planned improvements and features:

* Improve download speeds.
* Add a download queue for downloading multiple files.
* Display preview icons alongside file entries.
* Continue improving UI responsiveness and overall stability.

## Credits

* Built using the **devkitPro** ecosystem.
* UI powered by **Citro2D** and **Citro3D**.
* ROM databases parsed from standard release archives.
* **Antigravity/Codex**, because I barely know what I'm doing.
* Thanks to everyone in the homebrew community who tests, reports issues, and contributes ideas.
