#include <Arduino.h>

#include "ComponentMain.h"
#include "BombComponent.h"
#include "BombConfig.h"
#include "GameEvent.h"
#include "NeopixelModuleLedDriver.h"
#include "Adafruit_NeoPixel.h"
#include "EnumFlagOperators.h"

class TemplateModule : public DefusableModule, NeopixelLedModuleTrait {
public:
	TemplateModule() {
		
	}

	const char* GetName() override {
		return "Module";
	}

	void Bootstrap() override {
		DefusableModule::Bootstrap();
	}

	ModuleLedDriver* GetModuleLedDriver() override {
		return GetNeopixelLedDriver();
	}

	void Reset() override {
		DefusableModule::Reset();
	}

	void Standby() override {
		DefusableModule::Standby();
	}

	void Arm() override {
		DefusableModule::Arm();
	}

	void ActiveUpdate() override {
		
	}

	void Configure() override {
		
	}

	void LoadConfiguration(BombConfig* config) override {
		
	}

	void LoadConfiguration(ModuleConfig* config) override {
		
	}

	const InfoStreamBuilderBase::VariableParam* GetVariableInfo() override {
		return BOMB_NO_VARIABLES;
	}

	bconf::SyncFlag GetSyncFlags() {
		return bconf::SYNC_NOTHING;
	}

	bconf::BombEventBit GetAcceptedEvents() override {
		return bconf::NONE_BITS;
	}
};

TemplateModule mod;

void setup() {
	ComponentMain::GetInstance()->Setup(&mod);
}

void loop() {
	ComponentMain::GetInstance()->Loop();
}