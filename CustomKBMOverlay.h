#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <deque>
#include <filesystem>
#include <algorithm>
#include <mutex>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

namespace fs = std::filesystem;

class CustomKBMOverlay : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow
{
public:
	virtual void onLoad() override;
	virtual void onUnload() override;

	// PluginSettingsWindow
	void RenderSettings() override;
	std::string GetPluginName() override { return "Custom KBM Overlay"; }
	void SetImGuiContext(uintptr_t ctx) override;

private:
	void Render(CanvasWrapper canvas);

	// Helper struct to hold pressed images for a single key
	struct KeyImages {
		std::shared_ptr<ImageWrapper> pressed;
		bool isPressed = false;
		float opacity = 0.0f;
	};

	std::map<std::string, KeyImages> keys;

	// Base overlay / design image
	std::shared_ptr<ImageWrapper> overlayImage;
	std::shared_ptr<ImageWrapper> outlinesImage;
	std::string currentOverlayPath;

	// Animated Backgrounds
	bool bIsAnimated = false;
	std::vector<std::shared_ptr<ImageWrapper>> backgroundFrames;
	std::unique_ptr<std::mutex> bgMutex;
	float bgFrameTimer = 0.0f;
	int currentFrameIndex = 0;
	std::string lastBgFolderStatus = "No folder loaded";

	std::shared_ptr<ImageWrapper> LoadImageTemplate(std::string filename);
	void LoadAllImages();
	void LoadOverlayImage(const std::string& relativePath);
	void LoadBackgroundSequence(const std::string& folderName);
	void OnSetVehicleInput(std::string eventName);
	std::string GetImageCVarName();

	std::chrono::steady_clock::time_point lastRenderTime;
	std::chrono::steady_clock::time_point startTime;

	// Game state 
	bool bIsSupersonic = false;
	bool bIsBoosting = false;

	// Cached CVars for performance
	std::shared_ptr<CVarWrapper> cvarX, cvarY, cvarScale;
	std::shared_ptr<CVarWrapper> cvarMasterOpacity, cvarDesignOpacity;
	std::shared_ptr<CVarWrapper> cvarRainbow, cvarHighlightColor, cvarFadeSpeed;
	std::shared_ptr<CVarWrapper> cvarReactiveRgb, cvarBoostColor, cvarSupersonicColor;
	std::shared_ptr<CVarWrapper> cvarShowKpm, cvarLayoutProfile;
	std::shared_ptr<CVarWrapper> cvarBgAnimation, cvarBgFolder, cvarBgFps;

	// KPM tracking
	std::deque<float> keyPressTimes;
};
