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
1. Go to the [Custom KBM Overlay](https://bakkesplugins.com/) page on BakkesPlugins.
2. Click **Install with BakkesMod**.
3. **CRITICAL:** The automated installer **only** installs the DLL. You **must** download the `CustomKBMOverlay_Assets.zip` from the [GitHub Releases](https://github.com/YourUsername/CustomKBMOverlay/releases) page.
4. Extract that ZIP into your BakkesMod data folder: `AppData/Roaming/bakkesmod/bakkesmod/data/`.
   - Your final path should look like: `.../data/CustomKBMOverlay/layouts/...`

## Animated Backgrounds
1. Place your PNG frames in `CustomKBMOverlay/backgrounds/[folder_name]/`.
2. In the F2 menu, check **Enable Background Animation**.
3. Enter your folder name and press **Enter**.
4. **Tip**: Use [ezgif.com/video-to-png](https://ezgif.com/video-to-png) to easily convert video clips into PNG sequences.
5. **Optimization**: Use `resize_frames.py` (included in source) to scale images to 708x379 for best performance.

## License
MIT License - feel free to use and modify for your own projects!
