#include <Arduino.h>

#include "ComponentMain.h"
#include "BombComponent.h"
#include "BombConfig.h"
#include "GameEvent.h"

class TestModule : public BombModule, public EventfulComponentTrait<TestModule> {
private:
	int m_BlinkInterval;
	const char* m_Var2;

	int m_LedState;

public:
	TestModule() : m_Var2{nullptr} {

	}

	const char* GetName() override {
		return "Testovací modul";
	}

	void Bootstrap() override {
		pinMode(13, OUTPUT);
	}

	void Reset() override {
		if (m_Var2) {
			delete m_Var2;
		}
	}

	void Standby() override {
		m_LedState = LOW;
		m_Events.CancelAll();
		Display();
	}

	static bool BlinkEventFunc(game::Event<TestModule>* event, TestModule* module) {
		module->m_LedState = !module->m_LedState;
		event
		->Then(game::CreateWaitEvent<TestModule>(module->m_BlinkInterval))
		->Then(new game::Event<TestModule>(BlinkEventFunc));
		return true;
	}

	void Arm() override {
		m_Events.Start(new game::Event<TestModule>(BlinkEventFunc));
	}

	void Update() override {
		m_Events.Update();
	}

	void Display() override {
		digitalWrite(13, m_LedState);
	}

	void OnEvent(uint8_t id, void* data) override {

	}

	void LoadConfiguration(ModuleConfig* config) override {
		m_BlinkInterval = config->GetInt("Interval blikání");
	}

	const InfoStreamBuilderBase::VariableParam* GetVariableInfo() override {
		return BOMB_VARIABLES_ARRAY(
			{"Interval blikání", VAR_INT}
		);
	}

	BombConfig::ModuleFlag GetModuleFlags() {
		return BombConfig::ModuleFlag::DEFUSABLE;
	}

	bconf::SyncFlag GetSyncFlags() {
		return bconf::FETCH_CONFIG;
	}

	bconf::BombEventBit GetAcceptedEvents() override {
		return bconf::STRIKE_BIT | bconf::TIMER_SYNC_BIT | bconf::LIGHTS_BITS;
	}
};

TestModule mod;

void setup() {
	ComponentMain::GetInstance()->Setup(&mod);
}

void loop() {
	ComponentMain::GetInstance()->Loop();
}