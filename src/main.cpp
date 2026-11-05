#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

namespace {
	// Is noclip currently switched on?
	bool g_noclip = false;

	// Was noclip used at any point during the current attempt?
	// (used by Safe Mode to decide whether progress may be saved)
	bool g_tainted = false;

	std::string labelID() {
		return "noclip-label"_spr;
	}

	bool safeModeBlocksSaving() {
		return g_tainted && Mod::get()->getSettingValue<bool>("safe-mode");
	}

	// Creates / updates / hides the NOCLIP label to match the current settings.
	void refreshLabel() {
		auto pl = PlayLayer::get();
		if (!pl || !pl->m_uiLayer) return;

		auto mod = Mod::get();
		bool show = g_noclip && mod->getSettingValue<bool>("label-enabled");

		auto label = typeinfo_cast<CCLabelBMFont*>(pl->m_uiLayer->getChildByID(labelID()));
		if (!label) {
			if (!show) return;
			// bigFont.fnt is the main Geometry Dash font (Pusab)
			label = CCLabelBMFont::create("NOCLIP", "bigFont.fnt");
			label->setID(labelID());
			pl->m_uiLayer->addChild(label, 100);
		}

		label->setVisible(show);
		if (!show) return;

		auto winSize = CCDirector::get()->getWinSize();
		auto position = mod->getSettingValue<std::string>("label-position");
		auto yOffset = static_cast<float>(mod->getSettingValue<int64_t>("label-y-offset"));

		float margin = 8.f;
		float anchorX = 0.5f;
		float x = winSize.width / 2.f;
		if (position == "Left") {
			anchorX = 0.f;
			x = margin;
		}
		else if (position == "Right") {
			anchorX = 1.f;
			x = winSize.width - margin;
		}

		label->setAnchorPoint({anchorX, 0.f});
		label->setPosition({x, yOffset});
		label->setScale(static_cast<float>(mod->getSettingValue<double>("label-scale")));
		label->setOpacity(static_cast<GLubyte>(mod->getSettingValue<int64_t>("label-opacity")));
		label->setColor(mod->getSettingValue<ccColor3B>("label-color"));
	}

	void toggleNoclip() {
		g_noclip = !g_noclip;
		if (g_noclip) g_tainted = true;

		refreshLabel();

		if (PlayLayer::get() && Mod::get()->getSettingValue<bool>("toggle-notification")) {
			Notification::create(
				g_noclip ? "Noclip: ON" : "Noclip: OFF",
				g_noclip ? NotificationIcon::Success : NotificationIcon::None,
				0.6f
			)->show();
		}
	}
}

class $modify(NoclipPlayLayer, PlayLayer) {
	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		g_tainted = g_noclip;
		refreshLabel();
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		if (m_isPracticeMode) {
			// checkpoints may have been placed with noclip, so keep the flag
			g_tainted = g_tainted || g_noclip;
		}
		else {
			g_tainted = g_noclip;
		}
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		// Noclip: ignore every death source except the anti-cheat spike
		if (g_noclip && object != m_anticheatSpike) {
			g_tainted = true;
			return;
		}

		// Safe Mode: let the game think this is a test run so nothing gets saved
		if (safeModeBlocksSaving()) {
			auto prev = m_isTestMode;
			m_isTestMode = true;
			PlayLayer::destroyPlayer(player, object);
			m_isTestMode = prev;
			return;
		}

		PlayLayer::destroyPlayer(player, object);
	}

	void levelComplete() {
		if (safeModeBlocksSaving()) {
			auto prev = m_isTestMode;
			m_isTestMode = true;
			PlayLayer::levelComplete();
			m_isTestMode = prev;

			Notification::create(
				"Noclip safe mode: progress not saved",
				NotificationIcon::Warning,
				2.f
			)->show();
			return;
		}

		PlayLayer::levelComplete();
	}
};

$on_game(Loaded) {
	// Keybind chosen by the user in the mod's settings
	listenForKeybindSettingPresses("noclip-keybind", [](Keybind const&, bool down, bool repeat, double) {
		if (down && !repeat) {
			toggleNoclip();
		}
	});
}

$on_mod(Loaded) {
	// Update the label live when any label setting is changed
	listenForSettingChanges("label-enabled", [](bool) { refreshLabel(); });
	listenForSettingChanges("label-opacity", [](int64_t) { refreshLabel(); });
	listenForSettingChanges("label-scale", [](double) { refreshLabel(); });
	listenForSettingChanges("label-color", [](ccColor3B) { refreshLabel(); });
	listenForSettingChanges("label-position", [](std::string) { refreshLabel(); });
	listenForSettingChanges("label-y-offset", [](int64_t) { refreshLabel(); });
}
