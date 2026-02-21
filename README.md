# Custom KBM Overlay for BakkesMod

A highly customizable Keyboard and Mouse (KBM) overlay for Rocket League, featuring game-reactive RGB, animated backgrounds, and layout profiles.

![Preview Image](https://raw.githubusercontent.com/YourUsername/CustomKBMOverlay/main/preview.png)

## Features
* **Layout Profiles**: Switch between `Full`, `WASD`, and `Mouse Only` layouts instantly.
* **Reactive RGB**: Highlights change color based on game state (Supersonic, Boosting).
* **Animated Backgrounds**: Load PNG sequences from a folder for smooth, loopable animations.
* **KPM Counter**: Real-time Keys Per Minute tracking.
* **Natural Sorting**: Frame sequences are loaded in numerical order (1, 2, 10 instead of 1, 10, 2).
* **Fully Customizable**: Adjust position, scale, opacity, and custom colors via the F2 menu.

## Installation
1. Download the latest `CustomKBMOverlay.dll` from the [Releases](link_to_releases) page.
2. Place the DLL in your BakkesMod plugins folder: `AppData/Roaming/bakkesmod/bakkesmod/plugins/`.
3. Extract the assets (folders: `layouts`, `backgrounds`) into `AppData/Roaming/bakkesmod/bakkesmod/data/CustomKBMOverlay/`.

## Animated Backgrounds
1. Place your PNG frames in `CustomKBMOverlay/backgrounds/[folder_name]/`.
2. In the F2 menu, check **Enable Background Animation**.
3. Enter your folder name and press **Enter**.
4. **Tip**: Use `resize_frames.py` (included in source) to scale images to 708x379 for best performance.

## Build Requirements
* Visual Studio 2022
* BakkesMod SDK
* `Shlwapi.lib` (standard Windows library)

## License
MIT License - feel free to use and modify for your own projects!
