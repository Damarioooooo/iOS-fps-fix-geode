#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
	// cached settings so nothing is looked up every frame
	bool g_lockInterval = true;
	int64_t g_targetFps = 60;
	bool g_smoothDelta = false;

	void applyInterval(bool force) {
		if (!g_lockInterval) return;
		double want = 1.0 / static_cast<double>(std::max<int64_t>(1, g_targetFps));
		auto director = CCDirector::get();
		if (force || std::fabs(director->getAnimationInterval() - want) > 1e-6) {
			director->setAnimationInterval(want);
		}
	}
}

$on_mod(Loaded) {
	g_lockInterval = Mod::get()->getSettingValue<bool>("lock-interval");
	g_targetFps = Mod::get()->getSettingValue<int64_t>("target-fps");
	g_smoothDelta = Mod::get()->getSettingValue<bool>("smooth-delta");

	listenForSettingChanges<bool>("lock-interval", [](bool value) {
		g_lockInterval = value;
		applyInterval(true);
	});
	listenForSettingChanges<int64_t>("target-fps", [](int64_t value) {
		g_targetFps = value;
		applyInterval(true);
	});
	listenForSettingChanges<bool>("smooth-delta", [](bool value) {
		g_smoothDelta = value;
	});
}

class $modify(SFPlayLayer, PlayLayer) {
	struct Fields {
		double ema = 0.0;
		int frameCounter = 0;
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		applyInterval(true);
		return true;
	}

	void resume() {
		PlayLayer::resume();
		applyInterval(true);
	}

	void update(float dt) {
		if (g_smoothDelta && dt > 0.f) {
			double target = 1.0 / static_cast<double>(std::max<int64_t>(1, g_targetFps));
			// only smooth ordinary frames, leave big hitches (loading, pausing) alone
			if (dt < target * 3.0) {
				auto& ema = m_fields->ema;
				ema = (ema == 0.0) ? dt : ema * 0.9 + dt * 0.1;
				dt = static_cast<float>(dt * 0.5 + ema * 0.5);
			}
		}
		PlayLayer::update(dt);
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		// about once a second at 60fps, re-apply if something changed the interval
		if (++m_fields->frameCounter >= 60) {
			m_fields->frameCounter = 0;
			applyInterval(false);
		}
	}
};
