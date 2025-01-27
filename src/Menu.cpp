#include "Menu.hpp"
#include "Config.hpp"
#include "VR.hpp"
#include <openvr.h>

#include <array>
#include <format>

extern IDirect3DDevice9* gD3Ddev;
extern std::unique_ptr<VRInterface> gVR;
extern Config gCfg;
extern Config gSavedCfg;
extern bool gDrawOverlayBorder;

// Helper function to create an identity function to return its argument
template <typename T>
auto id(const T& txt) -> auto
{
    return [&] { return txt; };
}

void SelectMenu(size_t menuIdx);

LicenseMenu::LicenseMenu()
    : Menu("openRBRVR licenses", {})
    , menuScroll(0)
{
    for (const auto& row : gLicenseInformation) {
        this->entries.push_back({ .text = id(row), .font = IRBRGame::EFonts::FONT_SMALL, .menuColor = IRBRGame::EMenuColors::MENU_TEXT });
    }
}
void LicenseMenu::Left()
{
    SelectMenu(0);
}
void LicenseMenu::Select()
{
    SelectMenu(0);
}

void RecenterVR()
{
    auto chaperone = vr::VRChaperone();
    if (chaperone) {
        chaperone->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
    }
}

static std::string GetHorizonLockStr()
{
    switch (gCfg.lockToHorizon) {
        case HorizonLock::LOCK_NONE:
            return "Off";
        case HorizonLock::LOCK_ROLL:
            return "Roll";
        case HorizonLock::LOCK_PITCH:
            return "Pitch";
        case (HorizonLock::LOCK_ROLL | HorizonLock::LOCK_PITCH):
            return "Pitch and roll";
        default:
            return "Unknown";
    }
}

static void ChangeHorizonLock(bool forward)
{
    switch (gCfg.lockToHorizon) {
        case HorizonLock::LOCK_NONE:
            gCfg.lockToHorizon = forward ? HorizonLock::LOCK_ROLL : static_cast<HorizonLock>((HorizonLock::LOCK_ROLL | HorizonLock::LOCK_PITCH));
            break;
        case HorizonLock::LOCK_ROLL:
            gCfg.lockToHorizon = forward ? HorizonLock::LOCK_PITCH : HorizonLock::LOCK_NONE;
            break;
        case HorizonLock::LOCK_PITCH:
            gCfg.lockToHorizon = forward ? static_cast<HorizonLock>((HorizonLock::LOCK_ROLL | HorizonLock::LOCK_PITCH)) : HorizonLock::LOCK_ROLL;
            break;
        case (HorizonLock::LOCK_ROLL | HorizonLock::LOCK_PITCH):
            gCfg.lockToHorizon = forward ? HorizonLock::LOCK_NONE : HorizonLock::LOCK_PITCH;
            break;
        default:
            gCfg.lockToHorizon = HorizonLock::LOCK_NONE;
            break;
    }
}

static void ChangeRenderIn3dSettings(bool forward)
{
    if (forward && gCfg.renderMainMenu3d) {
        gCfg.renderMainMenu3d = false;
        gCfg.renderPreStage3d = false;
        gCfg.renderPauseMenu3d = false;
    } else if (forward) {
        gCfg.renderMainMenu3d = gCfg.renderPreStage3d;
        gCfg.renderPreStage3d = gCfg.renderPauseMenu3d;
        gCfg.renderPauseMenu3d = true;
    } else {
        gCfg.renderPauseMenu3d = gCfg.renderPreStage3d;
        gCfg.renderPreStage3d = gCfg.renderMainMenu3d;
        gCfg.renderMainMenu3d = false;
    }
}

static void ChangeCompanionMode(bool forward)
{
    if (forward) {
        if (gCfg.companionMode == CompanionMode::Off) {
            gCfg.companionMode = CompanionMode::VREye;
        } else if (gCfg.companionMode == CompanionMode::VREye) {
            gCfg.companionMode = CompanionMode::Static;
        } else {
            gCfg.companionMode = CompanionMode::Off;
        }
    } else {
        if (gCfg.companionMode == CompanionMode::Off) {
            gCfg.companionMode = CompanionMode::Static;
        } else if (gCfg.companionMode == CompanionMode::Static) {
            gCfg.companionMode = CompanionMode::VREye;
        } else {
            gCfg.companionMode = CompanionMode::Off;
        }
    }
}

void Toggle(bool& value) { value = !value; }

// clang-format off
static class Menu mainMenu = { "openRBRVR", {
  { .text = id("Recenter VR view"), .longText = {"Recenters VR view"}, .menuColor = IRBRGame::EMenuColors::MENU_TEXT, .position = Menu::menuItemsStartPos, .selectAction = RecenterVR },
  { .text = id("Horizon lock settings") , .longText = {"Horizon lock settings"}, .selectAction = [] { SelectMenu(4); } },
  { .text = id("Rendering settings") , .longText = {"Selection of different rendering settings"}, .selectAction = [] { SelectMenu(1); } },
  { .text = id("Overlay settings") , .longText = {"Adjust the size and position of the 2D content shown on", "top of the 3D view while driving."}, .selectAction = [] { SelectMenu(5); } },
  { .text = id("Desktop window settings") ,
    .longText = {"Adjust the size and position of the image shown", "on the desktop window while driving."},
    .color = [] { return (gVR) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .selectAction = [] { if(gVR) SelectMenu(6); }
  },
  { .text = id("Debug settings"), .longText = {"Not intended to be changed unless there is a problem that needs more information."}, .selectAction = [] { SelectMenu(2); } },
  { .text = [] { return std::format("VR runtime: {}", gCfg.runtime == OPENVR ? "OpenVR (SteamVR)" : (gCfg.wmrWorkaround ? "OpenXR (Reverb compatibility mode)" : "OpenXR")); },
    .longText {
        "Selects VR runtime. Requires game restart.",
        "",
        "SteamVR support is more mature and supports more devices.",
        "", "OpenXR is an open-source, royalty-free standard.",
        "It has less overhead and may result in better performance.",
        "OpenXR device compatibility is more limited for old 32-bit games like RBR.",
        "The performance of Reverb compatibility mode is worse than normal OpenXR.",
        "Use it only if there's problems with the normal mode."},
    .leftAction = [] {
        if (gCfg.runtime == OPENXR) { if(gCfg.wmrWorkaround) Toggle(gCfg.wmrWorkaround); else gCfg.runtime = OPENVR; }
        else { gCfg.runtime = OPENXR; gCfg.wmrWorkaround = true; }
    },
    .rightAction = [] {
        if (gCfg.runtime == OPENXR) { if(gCfg.wmrWorkaround) gCfg.runtime = OPENVR; else gCfg.wmrWorkaround = true; }
        else { gCfg.runtime = OPENXR; gCfg.wmrWorkaround = false; }
    },
    .selectAction = [] {},
  },
  { .text = id("Licenses"), .longText = {"License information of open source libraries used in the plugin's implementation."}, .selectAction = [] { SelectMenu(3); } },
  { .text = id("Save the current config to openRBRVR.toml"),
    .color = [] { return (gCfg == gSavedCfg) ? std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f) : std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f); },
    .selectAction = [] {
        if (gCfg.Write("Plugins\\openRBRVR.toml")) {
            gSavedCfg = gCfg;
        }
    }
  },
}};

static class Menu graphicsMenu = { "openRBRVR rendering settings", {
  { .text = [] { return std::format("Render loading screen: {}", gCfg.drawLoadingScreen ? "ON" : "OFF"); },
    .longText = { "If the screen flickers while loading, turn this OFF to have a black screen while loading."},
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menuItemsStartPos,
    .leftAction = [] { Toggle(gCfg.drawLoadingScreen); },
    .rightAction = [] { Toggle(gCfg.drawLoadingScreen); },
    .selectAction = [] { Toggle(gCfg.drawLoadingScreen); },
  },
  { .text = [] { return std::format("Render pause menu in 3D: {}", gCfg.renderPauseMenu3d ? "ON" : "OFF"); },
    .longText = { "Enable to render the pause menu of the game in 3D overlay instead of a 2D plane." },
    .leftAction = [] { Toggle(gCfg.renderPauseMenu3d); },
    .rightAction = [] { Toggle(gCfg.renderPauseMenu3d); },
    .selectAction = [] { Toggle(gCfg.renderPauseMenu3d); },
  },
  { .text = [] { return std::format("Render prestage animation in 3D: {}", gCfg.renderPreStage3d ? "ON" : "OFF"); },
    .longText = { "Enable to render the spinning animation around the car of the game in 3D instead of a 2D plane.", "Enabling this option may cause discomfort to some people."},
    .leftAction = [] { Toggle(gCfg.renderPreStage3d); },
    .rightAction = [] { Toggle(gCfg.renderPreStage3d); },
    .selectAction = [] { Toggle(gCfg.renderPreStage3d); },
  },
  { .text = [] { return std::format("Render main menu in 3D: {}", gCfg.renderMainMenu3d ? "ON" : "OFF"); },
    .longText = { "Enable to render the main menu in 3D instead of 2D plane.", "Enabling this option may cause discomfort to some people."},
    .leftAction = [] { Toggle(gCfg.renderMainMenu3d); },
    .rightAction = [] { Toggle(gCfg.renderMainMenu3d); },
    .selectAction = [] { Toggle(gCfg.renderMainMenu3d); },
  },
  { .text = [] { return std::format("Render replays in 3D: {}", gCfg.renderReplays3d ? "ON" : "OFF"); },
    .longText = { "Enable to render replays in 3D instead of a 2D plane." },
    .leftAction = [] { Toggle(gCfg.renderReplays3d); },
    .rightAction = [] { Toggle(gCfg.renderReplays3d); },
    .selectAction = [] { Toggle(gCfg.renderReplays3d); },
  },
  { .text = id("Back to previous menu"),
	.selectAction = [] { SelectMenu(0); }
  },
}};

static class Menu debugMenu = { "openRBRVR debug settings", {
  { .text = [] { return std::format("Debug information: {}", gCfg.debug ? "ON" : "OFF"); },
    .longText = { "Show a lot of technical information on the top-left of the screen." },
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
	.position = Menu::menuItemsStartPos,
    .leftAction = [] { Toggle(gCfg.debug); gDrawOverlayBorder = gCfg.debug; },
    .rightAction = [] { Toggle(gCfg.debug); gDrawOverlayBorder = gCfg.debug; },
    .selectAction = [] { Toggle(gCfg.debug); gDrawOverlayBorder = gCfg.debug; },
  },
  { .text = id("Back to previous menu"),
	.selectAction = [] { SelectMenu(0); }
  },
}};

static LicenseMenu licenseMenu;

static class Menu horizonLockMenu = { "openRBRVR horizon lock settings", {
  { .text = [] { return std::format("Lock horizon: {}", GetHorizonLockStr()); },
    .longText = {
        "Enable to rotate the car around the headset instead of rotating the headset with the car.",
        "For some people, enabling this option gives a more comfortable VR experience.",
        "Roll means locking the left-right axis.",
        "Pitch means locking the front-back axis."
    },
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menuItemsStartPos,
    .leftAction = [] { ChangeHorizonLock(false); },
    .rightAction = [] { ChangeHorizonLock(true); },
    .selectAction = [] { ChangeHorizonLock(true); },
  },
  { .text = [] { return std::format("Percentage: {}%", static_cast<int>(gCfg.horizonLockMultiplier * 100.0)); },
    .longText = {
        "Amount of locking that's happening. 100 means the horizon is always level.",
    },
    .leftAction = [] { gCfg.horizonLockMultiplier = std::max<double>(0.05, (gCfg.horizonLockMultiplier * 100.0 - 5) / 100.0); },
    .rightAction = [] { gCfg.horizonLockMultiplier = std::min<double>(1.0, (gCfg.horizonLockMultiplier * 100.0 + 5) / 100.0); },
  },
  { .text = id("Back to previous menu"),
    .selectAction = [] { SelectMenu(0); }
  },
}};

static class Menu overlayMenu = { "openRBRVR overlay settings", {
  { .text = [] { return std::format("Menu size: {:.2f}", gCfg.menuSize); },
    .longText = { "Adjust menu size." },
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
	.position = Menu::menuItemsStartPos,
    .leftAction = [] { gCfg.menuSize = std::max<float>((gCfg.menuSize - 0.05f), 0.05f); },
    .rightAction = [] { gCfg.menuSize = std::min<float>((gCfg.menuSize + 0.05f), 10.0f); },
  },
  { .text = [] { return std::format("Overlay size: {:.2f}", gCfg.overlaySize); },
    .longText = { "Adjust overlay size. Overlay is the area where 2D content", "is rendered when driving."},
    .leftAction = [] { gCfg.overlaySize = std::max<float>((gCfg.overlaySize - 0.05f), 0.05f); },
    .rightAction = [] { gCfg.overlaySize = std::min<float>((gCfg.overlaySize + 0.05f), 10.0f); },
  },
  { .text = [] { return std::format("Overlay X position: {:.2f}", gCfg.overlayTranslation.x); },
    .longText = { "Adjust overlay position. Overlay is the area where 2D content", "is rendered when driving."},
    .leftAction = [] { gCfg.overlayTranslation.x -= 0.01f; },
    .rightAction = [] { gCfg.overlayTranslation.x += 0.01f; },
  },
  { .text = [] { return std::format("Overlay Y position: {:.2f}", gCfg.overlayTranslation.y); },
    .longText = { "Adjust overlay position. Overlay is the area where 2D content", "is rendered when driving."},
    .leftAction = [] { gCfg.overlayTranslation.y -= 0.01f; },
    .rightAction = [] { gCfg.overlayTranslation.y += 0.01f; },
  },
  { .text = [] { return std::format("Overlay Z position: {:.2f}", gCfg.overlayTranslation.z); },
    .longText = { "Adjust overlay position. Overlay is the area where 2D content", "is rendered when driving."},
    .leftAction = [] { gCfg.overlayTranslation.z -= 0.01f; },
    .rightAction = [] { gCfg.overlayTranslation.z += 0.01f; },
  },
  { .text = id("Back to previous menu"),
    .selectAction = [] { SelectMenu(0); },
  },
}};

static const auto windowStep = 1;
static class Menu companionMenu = { "openRBRVR desktop window settings", {
  { .text = [] { return std::format("Desktop window mode: {}", CompanionModeStrPretty(gCfg.companionMode)); },
    .longText = { "Choose what is visible on the desktop monitor while driving.", "Off: Don't draw anything", "VR view: Draw what is seen in the VR headset", "Bonnet camera: Use normal 2D bonnet camera"},
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menuItemsStartPos,
    .leftAction = [] { ChangeCompanionMode(false); },
    .rightAction = [] { ChangeCompanionMode(true); },
  },
  {.text = [] { return std::format("Desktop window rendering area size: {} ({:.0f}x{:.0f} pixels)", gCfg.companionSize, std::round(std::get<0>(gVR->GetRenderResolution(LeftEye)) * (gCfg.companionSize / 100.0)), std::round(std::get<1>(gVR->GetRenderResolution(LeftEye)) * (gCfg.companionSize / 100.0 / gVR->aspectRatio))); },
    .longText = { "Adjust rendering area size. The value is percentage of the width.", "For example, setting this to 50 will render half width of the full resolution", "resolution VR texture." },
    .color = [] { return (gCfg.companionMode == CompanionMode::VREye) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .leftAction = [] {
        gCfg.companionSize = std::clamp(gCfg.companionSize - windowStep, 10, 100);
        gCfg.companionOffset.x = std::min(gCfg.companionOffset.x, 100 - gCfg.companionSize);
        gCfg.companionOffset.y = std::min(gCfg.companionOffset.y, 100 - static_cast<int>(std::round(gCfg.companionSize / gVR->aspectRatio)));
        gVR->CreateCompanionWindowBuffer(gD3Ddev);
    },
    .rightAction = [] {
        gCfg.companionSize = std::clamp(gCfg.companionSize + windowStep, 10, 100);
        gCfg.companionOffset.x = std::min(gCfg.companionOffset.x, 100 - gCfg.companionSize);
        gCfg.companionOffset.y = std::min(gCfg.companionOffset.y, 100 - static_cast<int>(std::round(gCfg.companionSize / gVR->aspectRatio)));
        gVR->CreateCompanionWindowBuffer(gD3Ddev);
    },
  },
  {.text = [] { return std::format("X offset: {} ({:.0f} pixels)", gCfg.companionOffset.x, std::floor(std::get<0>(gVR->GetRenderResolution(LeftEye)) * gCfg.companionOffset.x / 100.0f)); },
    .longText = { "X offset in percents from the left side of the screen." },
    .color = [] { return (gCfg.companionMode == CompanionMode::VREye) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .leftAction = [] {
        gCfg.companionOffset.x = std::clamp(gCfg.companionOffset.x - windowStep, 0, 100 - gCfg.companionSize);
        gVR->CreateCompanionWindowBuffer(gD3Ddev);
    },
    .rightAction = [] {
        gCfg.companionOffset.x = std::clamp(gCfg.companionOffset.x + windowStep, 0, 100 - gCfg.companionSize);
        gVR->CreateCompanionWindowBuffer(gD3Ddev);
    },
  },
  { .text = [] { return std::format("Y offset: {} ({:.0f} pixels)", gCfg.companionOffset.y, std::floor(std::get<1>(gVR->GetRenderResolution(LeftEye)) * gCfg.companionOffset.y / 100.0f)); },
    .longText = { "Y offset in percents from the top of the screen." },
    .color = [] { return (gCfg.companionMode == CompanionMode::VREye) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .leftAction = [] {
        gCfg.companionOffset.y = std::clamp(gCfg.companionOffset.y - windowStep, 0, 100 - static_cast<int>(std::round(gCfg.companionSize / gVR->aspectRatio)));
		gVR->CreateCompanionWindowBuffer(gD3Ddev);
    },
    .rightAction = [] {
        gCfg.companionOffset.y = std::clamp(gCfg.companionOffset.y + windowStep, 0, 100 - static_cast<int>(std::round(gCfg.companionSize / gVR->aspectRatio)));
        gVR->CreateCompanionWindowBuffer(gD3Ddev);
    },
  },
  { .text = [] { return std::format("Eye: {}", gCfg.companionEye == LeftEye ? "Left eye" : "Right eye"); },
    .longText = { "Eye that is being used to render the desktop window." },
    .color = [] { return (gCfg.companionMode == CompanionMode::VREye) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .leftAction = [] { gCfg.companionEye = gCfg.companionEye == LeftEye ? RightEye : LeftEye; },
    .rightAction = [] { gCfg.companionEye = gCfg.companionEye == LeftEye ? RightEye : LeftEye; },
  },
  { .text = id("Back to previous menu"),
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
    .selectAction = [] { SelectMenu(0); },
  },
}};
// clang-format on

static auto menus = std::to_array<class Menu*>({
    &mainMenu,
    &graphicsMenu,
    &debugMenu,
    &licenseMenu,
    &horizonLockMenu,
    &overlayMenu,
    &companionMenu,
});

class Menu* gMenu = menus[0];

void SelectMenu(size_t menuIdx)
{
    gMenu = menus[menuIdx % menus.size()];
}
