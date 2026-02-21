#include "pch.h"
#include "CustomKBMOverlay.h"
#include <chrono>
#include <cmath>

BAKKESMOD_PLUGIN(CustomKBMOverlay, "Custom KBM Overlay", "1.0", PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void CustomKBMOverlay::onLoad()
{
	_globalCvarManager = cvarManager;
	startTime = std::chrono::steady_clock::now();
	bgMutex = std::make_unique<std::mutex>();

	// Position / scale
	cvarX = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_overlay_x",     "0.05", "X position of the KBM overlay (0.0 to 1.0)", true, true, 0.0f, true, 1.0f));
	cvarY = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_overlay_y",     "0.7",  "Y position of the KBM overlay (0.0 to 1.0)", true, true, 0.0f, true, 1.0f));
	cvarScale = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_overlay_scale", "1.0",  "Scale of the KBM overlay",                   true, true, 0.1f, true, 5.0f));

	// Opacity CVars
	cvarDesignOpacity = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_design_opacity", "0.85", "Opacity of the base design/template image (0.0 to 1.0)", true, true, 0.0f, true, 1.0f));
	cvarMasterOpacity = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_master_opacity", "1.0",  "Master opacity for the entire KBM overlay (0.0 to 1.0)", true, true, 0.0f, true, 1.0f));

	// Image path CVar
	cvarManager->registerCvar("kbm_overlay_image", "CustomKBMOverlay/keyboard_bg.png", "Path to the base keyboard design PNG (relative to bakkesmod/data/)");

	// Appearance CVars
	cvarRainbow = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_color_rainbow", "0", "Enable rainbow highlight color", true, true, 0, true, 1));
	cvarRainbow->bindTo(std::make_shared<bool>());
	cvarHighlightColor = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_highlight_color", "#00D250", "Custom highlight color of the pressed keys"));
	cvarFadeSpeed = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_fade_speed", "0.15", "Seconds for keys to fully fade out (0 = instant)", true, true, 0.0f, true, 2.0f));
	cvarLayoutProfile = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_layout_profile", "0", "Layout Profile (0=Full, 1=WASD, 2=Mouse)", true, true, 0, true, 2));

	// Animated Background CVars
	cvarBgAnimation = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_background_animation", "0", "Enable animated background sequence", true, true, 0, true, 1));
	cvarBgAnimation->bindTo(std::make_shared<bool>());
	cvarBgFolder = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_background_folder", "", "Folder name inside 'backgrounds/' containing PNG sequence"));
	cvarBgFps = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_background_fps", "24.0", "Frames per second for the background animation", true, true, 1.0f, true, 120.0f));

	cvarManager->registerCvar("kbm_overlay_image_full", "keyboard_bg.png", "Base design image for Full layout");
	cvarManager->registerCvar("kbm_overlay_image_wasd", "keyboard_bg.png", "Base design image for WASD layout");
	cvarManager->registerCvar("kbm_overlay_image_mouse", "keyboard_bg.png", "Base design image for Mouse layout");

	auto bgReload = [this](std::string varName, CVarWrapper cvar) {
		std::string bgPath = cvar.getStringValue();
		gameWrapper->Execute([this, bgPath](GameWrapper* gw) {
			LoadOverlayImage(bgPath);
		});
	};
	cvarManager->getCvar("kbm_overlay_image_full").addOnValueChanged(bgReload);
	cvarManager->getCvar("kbm_overlay_image_wasd").addOnValueChanged(bgReload);
	cvarManager->getCvar("kbm_overlay_image_mouse").addOnValueChanged(bgReload);

	auto animBgReload = [this](std::string varName, CVarWrapper cvar) {
		std::string folderName = cvar.getStringValue();
		gameWrapper->Execute([this, folderName](GameWrapper* gw) {
			LoadBackgroundSequence(folderName);
		});
	};
	cvarBgFolder->addOnValueChanged(animBgReload);

	cvarReactiveRgb = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_reactive_rgb", "1", "Override colors based on game state", true, true, 0, true, 1));
	cvarReactiveRgb->bindTo(std::make_shared<bool>());
	cvarBoostColor = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_boost_color", "#FF7800", "Color of the keys while Boosting"));
	cvarSupersonicColor = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_supersonic_color", "#00FFFF", "Color of the keys while Supersonic"));
	cvarShowKpm = std::make_shared<CVarWrapper>(cvarManager->registerCvar("kbm_show_kpm", "1", "Show Keys Per Minute counter", true, true, 0, true, 1));
	cvarShowKpm->bindTo(std::make_shared<bool>());

	// Hot-reload overlay image when path changes
	// This old hook is replaced by the new bgReload lambda for specific layout images
	// cvarManager->getCvar("kbm_overlay_image").addOnValueChanged([this](std::string, CVarWrapper cvar) {
	// 	std::string newPath = cvar.getStringValue();
	// 	gameWrapper->Execute([this, newPath](GameWrapper* gw) {
	// 		LoadOverlayImage(newPath);
	// 	});
	// });

	LoadAllImages();
	LoadOverlayImage(cvarManager->getCvar(GetImageCVarName()).getStringValue());
	LoadBackgroundSequence(cvarManager->getCvar("kbm_background_folder").getStringValue());

	lastRenderTime = std::chrono::steady_clock::now();

	gameWrapper->RegisterDrawable(std::bind(&CustomKBMOverlay::Render, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput", std::bind(&CustomKBMOverlay::OnSetVehicleInput, this, std::placeholders::_1));
}

void CustomKBMOverlay::onUnload()
{
	gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void CustomKBMOverlay::OnSetVehicleInput(std::string eventName)
{
	if (!gameWrapper->IsInGame() && !gameWrapper->IsInFreeplay()) return;

	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;

	CarWrapper car = gameWrapper->GetLocalCar();
	if (!car) return;

	bIsSupersonic = car.GetbSuperSonic();
	
	// CarWrapper doesn't have a direct bBoost flag, we have to get the attached BoostComponent
	BoostWrapper boost = car.GetBoostComponent();
	bIsBoosting = (boost && boost.GetbActive());
}

void CustomKBMOverlay::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

// ---------------------------------------------------------------------------
// Image loading helpers
// ---------------------------------------------------------------------------

std::string CustomKBMOverlay::GetImageCVarName()
{
	int profile = cvarManager->getCvar("kbm_layout_profile").getIntValue();
	if (profile == 1) return "kbm_overlay_image_wasd";
	if (profile == 2) return "kbm_overlay_image_mouse";
	return "kbm_overlay_image_full";
}

std::shared_ptr<ImageWrapper> CustomKBMOverlay::LoadImageTemplate(std::string filename)
{
	int profile = cvarManager->getCvar("kbm_layout_profile").getIntValue();
	std::string subDir = "";
	if (profile == 1) subDir = "layouts/wasd/";
	else if (profile == 2) subDir = "layouts/mouse/";

	std::string fullPath = "CustomKBMOverlay/" + subDir + filename;
	auto img = std::make_shared<ImageWrapper>(gameWrapper->GetDataFolder() / fullPath, true);
	return img;
}

void CustomKBMOverlay::LoadAllImages()
{
	std::vector<std::string> keyNames = {
		"esc", "1", "2", "3", "4", "5",
		"tab", "q", "w", "e", "r", "t",
		"caps", "a", "s", "d", "f", "g",
		"shift", "z", "x", "c", "v", "b",
		"ctrl", "alt", "space",
		"mouse_left", "mouse_right", "mouse_4", "mouse_5"
	};

	for (const auto& key : keyNames) {
		KeyImages kImages;
		kImages.pressed = LoadImageTemplate(key + "_pressed.png");
		kImages.isPressed = false;
		keys[key] = kImages;
	}
}

void CustomKBMOverlay::LoadOverlayImage(const std::string& relativePath)
{
	std::string filename = relativePath;
	if (filename.empty()) {
		if (auto cv = cvarManager->getCvar(GetImageCVarName())) {
			filename = cv.getStringValue();
		} else {
			filename = "keyboard_bg.png";
		}
	}
	
	// Strip legacy "CustomKBMOverlay/" prefix if user typed it, we just want the filename
	if (filename.find("CustomKBMOverlay/") == 0) {
		filename = filename.substr(17);
	}
	if (filename == "keyboard_template.png") filename = "keyboard_bg.png"; // Auto-upgrade

	int profile = cvarManager->getCvar("kbm_layout_profile").getIntValue();
	std::string subDir = "";
	if (profile == 1) subDir = "layouts/wasd/";
	else if (profile == 2) subDir = "layouts/mouse/";

	std::string finalPath = "CustomKBMOverlay/" + subDir + filename;

	overlayImage = std::make_shared<ImageWrapper>(gameWrapper->GetDataFolder() / finalPath, true);
	currentOverlayPath = finalPath;
	outlinesImage = LoadImageTemplate("keyboard_outlines.png");
}

void CustomKBMOverlay::LoadBackgroundSequence(const std::string& folderName)
{
	std::vector<std::shared_ptr<ImageWrapper>> tempFrames;
	
	if (folderName.empty()) {
		if (bgMutex) { std::lock_guard<std::mutex> lock(*bgMutex); }
		backgroundFrames.clear();
		currentFrameIndex = 0;
		bgFrameTimer = 0.0f;
		lastBgFolderStatus = "No folder entered.";
		return;
	}

	fs::path bgPath = gameWrapper->GetDataFolder() / "CustomKBMOverlay" / "backgrounds" / folderName;
	if (!fs::exists(bgPath)) {
		lastBgFolderStatus = "Path not found: /backgrounds/" + folderName;
		cvarManager->log("Background folder not found: " + bgPath.string());
		return;
	}
	if (!fs::is_directory(bgPath)) {
		lastBgFolderStatus = "Not a directory: " + folderName;
		return;
	}

	std::vector<fs::path> paths;
	for (const auto& entry : fs::directory_iterator(bgPath)) {
		if (entry.is_regular_file() && entry.path().extension() == ".png") {
			paths.push_back(entry.path());
			if (paths.size() >= 150) {
				cvarManager->log("Background frame limit reached (150). Skipping extra files.");
				break;
			}
		}
	}

	if (paths.empty()) {
		lastBgFolderStatus = "Folder found, but contains no .png files.";
		return;
	}

	std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b) {
		return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
	});

	for (const auto& p : paths) {
		tempFrames.push_back(std::make_shared<ImageWrapper>(p, true));
	}

	{
		if (bgMutex) { std::lock_guard<std::mutex> lock(*bgMutex); }
		backgroundFrames = std::move(tempFrames);
		currentFrameIndex = 0;
		bgFrameTimer = 0.0f;
		lastBgFolderStatus = "Success! Loaded " + std::to_string(backgroundFrames.size()) + " frames.";
	}
	
	cvarManager->log("Loaded " + std::to_string(backgroundFrames.size()) + " frames for background animation.");
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void CustomKBMOverlay::Render(CanvasWrapper canvas)
{
	keys["esc"].isPressed        = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
	keys["1"].isPressed          = (GetAsyncKeyState('1') & 0x8000) != 0;
	keys["2"].isPressed          = (GetAsyncKeyState('2') & 0x8000) != 0;
	keys["3"].isPressed          = (GetAsyncKeyState('3') & 0x8000) != 0;
	keys["4"].isPressed          = (GetAsyncKeyState('4') & 0x8000) != 0;
	keys["5"].isPressed          = (GetAsyncKeyState('5') & 0x8000) != 0;
	keys["tab"].isPressed        = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
	keys["q"].isPressed          = (GetAsyncKeyState('Q') & 0x8000) != 0;
	keys["w"].isPressed          = (GetAsyncKeyState('W') & 0x8000) != 0;
	keys["e"].isPressed          = (GetAsyncKeyState('E') & 0x8000) != 0;
	keys["r"].isPressed          = (GetAsyncKeyState('R') & 0x8000) != 0;
	keys["t"].isPressed          = (GetAsyncKeyState('T') & 0x8000) != 0;
	keys["caps"].isPressed       = (GetAsyncKeyState(VK_CAPITAL) & 0x8000) != 0;
	keys["a"].isPressed          = (GetAsyncKeyState('A') & 0x8000) != 0;
	keys["s"].isPressed          = (GetAsyncKeyState('S') & 0x8000) != 0;
	keys["d"].isPressed          = (GetAsyncKeyState('D') & 0x8000) != 0;
	keys["f"].isPressed          = (GetAsyncKeyState('F') & 0x8000) != 0;
	keys["g"].isPressed          = (GetAsyncKeyState('G') & 0x8000) != 0;
	keys["shift"].isPressed      = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	keys["z"].isPressed          = (GetAsyncKeyState('Z') & 0x8000) != 0;
	keys["x"].isPressed          = (GetAsyncKeyState('X') & 0x8000) != 0;
	keys["c"].isPressed          = (GetAsyncKeyState('C') & 0x8000) != 0;
	keys["v"].isPressed          = (GetAsyncKeyState('V') & 0x8000) != 0;
	keys["b"].isPressed          = (GetAsyncKeyState('B') & 0x8000) != 0;
	keys["ctrl"].isPressed       = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
	keys["alt"].isPressed        = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
	keys["space"].isPressed      = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	keys["mouse_left"].isPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	keys["mouse_right"].isPressed= (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	keys["mouse_4"].isPressed    = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
	keys["mouse_5"].isPressed    = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;

	// Calculate delta time for fade-out animations
	auto now = std::chrono::steady_clock::now();
	float dt = std::chrono::duration<float>(now - lastRenderTime).count();
	lastRenderTime = now;
	if (dt > 0.1f) dt = 0.016f; // Prevent huge spikes on load/alt-tab

	float fadeDuration = cvarFadeSpeed->getFloatValue();

	// Update opacities and track KPM
	float nowSec = (float)std::chrono::duration<double>(now.time_since_epoch()).count();
	// Precompute the target color for ripples
	LinearColor highlightColor = cvarHighlightColor->getColorValue();
	bool rainbow = cvarRainbow->getBoolValue();
	float rainbowSpeed = 0.5f;
	float rainbowSat = 1.0f;
	
	// Game state overrides (Supersonic/Boost)
	LinearColor baseColor = highlightColor;
	if (rainbow) {
		float hue = fmodf((float)nowSec * rainbowSpeed, 1.0f);
		float c = 1.0f * rainbowSat;
		float h = hue * 6.0f;
		float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
		float m = 1.0f - c;
		float rC = 0, gC = 0, bC = 0;
		if (h < 1) { rC = c; gC = x; }
		else if (h < 2) { rC = x; gC = c; }
		else if (h < 3) { gC = c; bC = x; }
		else if (h < 4) { gC = x; bC = c; }
		else if (h < 5) { rC = x; bC = c; }
		else { rC = c; bC = x; }
		baseColor = LinearColor(rC + m, gC + m, bC + m, 1.0f);
	}
	
	if (cvarReactiveRgb->getBoolValue()) {
		if (bIsSupersonic) {
			
			float pulse = (sinf((float)nowSec * 15.0f) + 1.0f) / 2.0f; 
			LinearColor deepBlue = LinearColor(0.0f, 0.2f, 1.0f, 1.0f);
			LinearColor brightCyan = cvarSupersonicColor->getColorValue();
			
			baseColor.R = deepBlue.R + (brightCyan.R - deepBlue.R) * pulse;
			baseColor.G = deepBlue.G + (brightCyan.G - deepBlue.G) * pulse;
			baseColor.B = deepBlue.B + (brightCyan.B - deepBlue.B) * pulse;
			baseColor.A = 1.0f;
		} 
		else if (bIsBoosting) {
			baseColor = cvarBoostColor->getColorValue();
		}
	}

	for (auto& pair : keys) {
		const std::string& keyName = pair.first;
		KeyImages& state = pair.second;

		if (state.isPressed) {
			if (state.opacity < 1.0f) {
				// Just pressed down
				keyPressTimes.push_back(nowSec);
			}
			state.opacity = 1.0f;
		} else if (fadeDuration > 0.001f) {
			state.opacity -= (1.0f / fadeDuration) * dt;
			if (state.opacity < 0.0f) state.opacity = 0.0f;
		} else {
			state.opacity = 0.0f;
		}
	}

	// Prune KPM queue older than 60 seconds
	while (!keyPressTimes.empty() && nowSec - keyPressTimes.front() > 60.0f) {
		keyPressTimes.pop_front();
	}

	// Shared positioning
	Vector2 screenSize = canvas.GetSize();
	float xPos  = cvarX->getFloatValue()     * screenSize.X;
	float yPos  = cvarY->getFloatValue()     * screenSize.Y;
	float scale = cvarScale->getFloatValue();

	float masterOpacity = cvarMasterOpacity->getFloatValue();
	float designOpacity = cvarDesignOpacity->getFloatValue();

	// 1. Draw base keyboard design
	bool animated = cvarBgAnimation->getBoolValue();
	if (animated) {
		if (bgMutex) { std::lock_guard<std::mutex> lock(*bgMutex); }
		if (!backgroundFrames.empty()) {
			float fps = cvarBgFps->getFloatValue();
			if (fps < 1.0f) fps = 1.0f;
			
			float elapsed = std::chrono::duration<float>(now - startTime).count();
			int frameCount = (int)backgroundFrames.size();
			int frameIdx = (int)(elapsed * fps) % frameCount;
			
			auto& frame = backgroundFrames[frameIdx];
			if (frame && frame->IsLoadedForCanvas()) {
				canvas.SetColor(255, 255, 255, (unsigned char)(designOpacity * masterOpacity * 255.0f));
				canvas.SetPosition(Vector2{ (int)xPos, (int)yPos });
				canvas.DrawTexture(frame.get(), scale);
			}
		}
		else if (overlayImage && overlayImage->IsLoadedForCanvas())
		{
			canvas.SetColor(255, 255, 255, (unsigned char)(designOpacity * masterOpacity * 255.0f));
			canvas.SetPosition(Vector2{ (int)xPos, (int)yPos });
			canvas.DrawTexture(overlayImage.get(), scale);
		}
	}
	else if (overlayImage && overlayImage->IsLoadedForCanvas())
	{
		canvas.SetColor(255, 255, 255, (unsigned char)(designOpacity * masterOpacity * 255.0f));
		canvas.SetPosition(Vector2{ (int)xPos, (int)yPos });
		canvas.DrawTexture(overlayImage.get(), scale);
	}

	// 1.5 Draw static lines and text on top of the design
	if (outlinesImage && outlinesImage->IsLoadedForCanvas())
	{
		unsigned char outlineAlpha = static_cast<unsigned char>(masterOpacity * 255.0f);
		canvas.SetColor(255, 255, 255, outlineAlpha);
		canvas.SetPosition(Vector2{ (int)xPos, (int)yPos });
		canvas.DrawTexture(outlinesImage.get(), scale);
	}

	// 2. Draw per-key highlight images on top (only if pressed)
	unsigned char keyR = (unsigned char)highlightColor.R;
	unsigned char keyG = (unsigned char)highlightColor.G;
	unsigned char keyB = (unsigned char)highlightColor.B;

	if (rainbow) {
		float time = (float)std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
		float hue = fmodf(time * 0.5f, 1.0f); // 0.5 cycles per second
		float h = hue * 6.0f;
		float x = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
		float rf = 0, gf = 0, bf = 0;
		if (h < 1) { rf = 1; gf = x; bf = 0; }
		else if (h < 2) { rf = x; gf = 1; bf = 0; }
		else if (h < 3) { rf = 0; gf = 1; bf = x; }
		else if (h < 4) { rf = 0; gf = x; bf = 1; }
		else if (h < 5) { rf = x; gf = 0; bf = 1; }
		else { rf = 1; gf = 0; bf = x; }
		keyR = (unsigned char)(rf * 255.0f);
		keyG = (unsigned char)(gf * 255.0f);
		keyB = (unsigned char)(bf * 255.0f);
	}

	for (auto const& pair : keys)
	{
		const KeyImages& state = pair.second;
		if (state.opacity > 0.001f && state.pressed && state.pressed->IsLoadedForCanvas())
		{
			unsigned char r = keyR, g = keyG, b = keyB;
			if (cvarReactiveRgb->getBoolValue()) {
				// User priority: Supersonic overrides Boost visually
				if (bIsSupersonic) {
					LinearColor mc = cvarSupersonicColor->getColorValue();
					float pulse = (sinf(nowSec * 15.0f) + 1.0f) * 0.5f; // 0.0 to 1.0 fast pulse
					float multiplier = 0.2f + (0.8f * pulse); // Dips down to 20% brightness
					r = (unsigned char)(mc.R * multiplier); 
					g = (unsigned char)(mc.G * multiplier); 
					b = (unsigned char)(mc.B * multiplier); 
				} else if (bIsBoosting) {
					LinearColor mc = cvarBoostColor->getColorValue();
					r = (unsigned char)mc.R; g = (unsigned char)mc.G; b = (unsigned char)mc.B; 
				}
			}

			unsigned char a = static_cast<unsigned char>(masterOpacity * state.opacity * 255.0f);
			canvas.SetColor(r, g, b, a);
			canvas.SetPosition(Vector2{ (int)xPos, (int)yPos });
			canvas.DrawTexture(state.pressed.get(), scale);
		}
	}

	// 4. Draw KPM counter if enabled
	if (cvarShowKpm->getBoolValue()) {
		int kpm = (int)keyPressTimes.size();
		canvas.SetColor(255, 255, 255, static_cast<unsigned char>(masterOpacity * 255.0f));
		canvas.SetPosition(Vector2{ (int)xPos, (int)yPos - (int)(30 * scale) });
		
		std::string kpmText = "KPM: " + std::to_string(keyPressTimes.size());
		canvas.DrawString(kpmText, scale * 2.0f, scale * 2.0f);
	}
}

// ---------------------------------------------------------------------------
// Plugin Settings UI (ImGui)
// ---------------------------------------------------------------------------

void CustomKBMOverlay::RenderSettings()
{
	// --- Layout Profile ---
	ImGui::TextUnformatted("Layout Profile");
	int layoutProfile = cvarManager->getCvar("kbm_layout_profile").getIntValue();
	const char* layouts[] = { "Full Keyboard + Mouse", "WASD Only", "Mouse Only" };
	ImGui::SetNextItemWidth(250.0f);
	if (ImGui::Combo("##layout", &layoutProfile, layouts, IM_ARRAYSIZE(layouts))) {
		cvarManager->getCvar("kbm_layout_profile").setValue(layoutProfile);
		
		std::string cvName = GetImageCVarName();
		std::string bgName = cvarManager->getCvar(cvName) ? cvarManager->getCvar(cvName).getStringValue() : "keyboard_bg.png";
		gameWrapper->Execute([this, bgName](GameWrapper* gw) {
			LoadAllImages();
			LoadOverlayImage(bgName);
		});
	}
	ImGui::Spacing();

	// --- Image path ---
	ImGui::TextUnformatted("Base keyboard design image");
	ImGui::SameLine();
	ImGui::TextDisabled("(relative to bakkesmod/data/)");

	static char pathBuf[512] = {};
	static bool firstFrame = true;
	std::string cvName = GetImageCVarName();
	std::string currentPath = cvarManager->getCvar(cvName) ? cvarManager->getCvar(cvName).getStringValue() : "keyboard_bg.png";

	// Initialize buffer once, or if the CVar changes externally
	if (firstFrame || (std::string(pathBuf).empty() && !currentPath.empty()) || 
		(!ImGui::IsAnyItemActive() && currentPath != std::string(pathBuf))) 
	{
		strncpy_s(pathBuf, currentPath.c_str(), sizeof(pathBuf) - 1);
		firstFrame = false;
	}

	ImGui::SetNextItemWidth(400.0f);
	if (ImGui::InputText("##overlay_path", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		cvarManager->getCvar(cvName).setValue(std::string(pathBuf));
	}

	ImGui::SameLine();
	if (ImGui::Button("Reload"))
	{
		cvarManager->getCvar(cvName).setValue(std::string(pathBuf));
	}

	ImGui::Separator();

	// --- Design opacity ---
	float designOpacity = cvarManager->getCvar("kbm_design_opacity").getFloatValue();
	ImGui::TextUnformatted("Design opacity");
	ImGui::SetNextItemWidth(300.0f);
	if (ImGui::SliderFloat("##design_opacity", &designOpacity, 0.0f, 1.0f, "%.2f"))
		cvarManager->getCvar("kbm_design_opacity").setValue(designOpacity);
	ImGui::SameLine();
	ImGui::TextDisabled("Base image only");

	// --- Master opacity ---
	float masterOpacity = cvarManager->getCvar("kbm_master_opacity").getFloatValue();
	ImGui::TextUnformatted("Master opacity");
	ImGui::SetNextItemWidth(300.0f);
	if (ImGui::SliderFloat("##master_opacity", &masterOpacity, 0.0f, 1.0f, "%.2f"))
		cvarManager->getCvar("kbm_master_opacity").setValue(masterOpacity);
	ImGui::SameLine();
	ImGui::TextDisabled("Entire overlay");

	ImGui::Separator();

	// --- Colors ---
	ImGui::TextUnformatted("Highlights");
	
	bool reactive = cvarManager->getCvar("kbm_reactive_rgb").getBoolValue();
	if (ImGui::Checkbox("Game-Reactive RGB", &reactive)) {
		cvarManager->getCvar("kbm_reactive_rgb").setValue(reactive);
	}

	if (reactive) {
		ImGui::Indent(20.0f);
		
		LinearColor boostCol = cvarManager->getCvar("kbm_boost_color").getColorValue();
		float bC[3] = { boostCol.R / 255.0f, boostCol.G / 255.0f, boostCol.B / 255.0f };
		if (ImGui::ColorEdit3("Boost Color", bC)) {
			char hex[32]; snprintf(hex, sizeof(hex), "#%02X%02X%02X", (int)(bC[0]*255), (int)(bC[1]*255), (int)(bC[2]*255));
			cvarManager->getCvar("kbm_boost_color").setValue(std::string(hex));
		}

		LinearColor ssCol = cvarManager->getCvar("kbm_supersonic_color").getColorValue();
		float sC[3] = { ssCol.R / 255.0f, ssCol.G / 255.0f, ssCol.B / 255.0f };
		if (ImGui::ColorEdit3("Supersonic Color", sC)) {
			char hex[32]; snprintf(hex, sizeof(hex), "#%02X%02X%02X", (int)(sC[0]*255), (int)(sC[1]*255), (int)(sC[2]*255));
			cvarManager->getCvar("kbm_supersonic_color").setValue(std::string(hex));
		}

		ImGui::Unindent(20.0f);
	}

	bool rainbow = cvarManager->getCvar("kbm_color_rainbow").getBoolValue();
	if (ImGui::Checkbox("Rainbow Mode", &rainbow)) {
		cvarManager->getCvar("kbm_color_rainbow").setValue(rainbow);
	}

	if (!rainbow) {
		LinearColor color = cvarManager->getCvar("kbm_highlight_color").getColorValue();
		float col[3] = { color.R / 255.0f, color.G / 255.0f, color.B / 255.0f };
		if (ImGui::ColorEdit3("Custom Color", col)) {
			char hex[32];
			snprintf(hex, sizeof(hex), "#%02X%02X%02X", (int)(col[0]*255), (int)(col[1]*255), (int)(col[2]*255));
			cvarManager->getCvar("kbm_highlight_color").setValue(std::string(hex));
		}
	}

	ImGui::Separator();

	ImGui::Separator();

	// --- Animated Backgrounds ---
	ImGui::TextUnformatted("Animated Background (Image Sequence)");
	
	bool animEnabled = cvarManager->getCvar("kbm_background_animation").getBoolValue();
	if (ImGui::Checkbox("Enable Background Animation", &animEnabled)) {
		cvarManager->getCvar("kbm_background_animation").setValue(animEnabled);
	}

	if (animEnabled) {
		ImGui::Indent(20.0f);

		static char folderBuf[128] = "";
		// Initialize buffer if empty and cvar has value
		if (folderBuf[0] == '\0') {
			std::string currentFolder = cvarManager->getCvar("kbm_background_folder").getStringValue();
			strncpy_s(folderBuf, currentFolder.c_str(), sizeof(folderBuf));
		}

		if (ImGui::InputText("Folder Name", folderBuf, sizeof(folderBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
			cvarManager->getCvar("kbm_background_folder").setValue(std::string(folderBuf));
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Folder inside 'bakkesmod/data/CustomKBMOverlay/backgrounds/'\nPress Enter to reload sequence.");
		}

		float animFps = cvarManager->getCvar("kbm_background_fps").getFloatValue();
		if (ImGui::SliderFloat("Animation FPS", &animFps, 1.0f, 120.0f, "%.1f")) {
			cvarManager->getCvar("kbm_background_fps").setValue(animFps);
		}

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: %s", lastBgFolderStatus.c_str());

		if (backgroundFrames.size() > 50) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
			ImGui::TextWrapped("Warning: Long sequences use extreme VRAM!");
			ImGui::PopStyleColor();
			ImGui::TextWrapped("Please scale images to ~800x420 to prevent crashes.");
		}
	}

	ImGui::Separator();

	// --- Animations & Tracking ---
	ImGui::TextUnformatted("Animations & Tracking");

	bool showKpm = cvarManager->getCvar("kbm_show_kpm").getBoolValue();
	if (ImGui::Checkbox("Show KPM Counter", &showKpm)) {
		cvarManager->getCvar("kbm_show_kpm").setValue(showKpm);
	}

	float fadeDuration = cvarManager->getCvar("kbm_fade_speed").getFloatValue();
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::SliderFloat("Fade Out Duration", &fadeDuration, 0.0f, 2.0f, "%.2f sec"))
		cvarManager->getCvar("kbm_fade_speed").setValue(fadeDuration);
	ImGui::SameLine();
	ImGui::TextDisabled("(0 = instant off)");

	ImGui::Separator();

	// --- Position / scale ---
	float xVal = cvarManager->getCvar("kbm_overlay_x").getFloatValue();
	float yVal = cvarManager->getCvar("kbm_overlay_y").getFloatValue();
	float sVal = cvarManager->getCvar("kbm_overlay_scale").getFloatValue();

	ImGui::TextUnformatted("Overlay position & scale");

	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::SliderFloat("X position##x", &xVal, 0.0f, 1.0f, "%.2f"))
		cvarManager->getCvar("kbm_overlay_x").setValue(xVal);

	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::SliderFloat("Y position##y", &yVal, 0.0f, 1.0f, "%.2f"))
		cvarManager->getCvar("kbm_overlay_y").setValue(yVal);

	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::SliderFloat("Scale##scale", &sVal, 0.1f, 5.0f, "%.2f"))
		cvarManager->getCvar("kbm_overlay_scale").setValue(sVal);
}
