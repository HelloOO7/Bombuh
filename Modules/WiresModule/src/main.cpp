#include <Arduino.h>

#include "ComponentMain.h"
#include "BombComponent.h"
#include "BombConfig.h"
#include "GameEvent.h"
#include "NeopixelModuleLedDriver.h"

static const ModuleInfoStreamBuilder::VariableParam* WIRE_VARS = BOMB_VARIABLES_ARRAY(
	{"Drát 1", VAR_STR_ENUM},
	{"Drát 2", VAR_STR_ENUM},
	{"Drát 3", VAR_STR_ENUM},
	{"Drát 4", VAR_STR_ENUM},
	{"Drát 5", VAR_STR_ENUM},
	{"Drát 6", VAR_STR_ENUM}
);

enum WireColor {
	WIRE_NOT_PRESENT,
	WIRE_BLACK,
	WIRE_BLUE,
	WIRE_RED,
	WIRE_WHITE,
	WIRE_YELLOW,

	WIRE_COLOR_MAX
};

static const char* WIRE_COLOR_NAMES[] = {
	"",
	"Black",
	"Blue",
	"Red",
	"White",
	"Yellow"
};

class WiresModule : public DefusableModule, NeopixelLedModuleTrait {
private:
	static constexpr int WIRE_COUNT = 6;
	static constexpr int WIRE_0_PIN = 9;
	static constexpr int WIRE_5_PIN = 4;

	int m_WireColors[WIRE_COUNT];
	int m_WireLut[WIRE_COUNT];
	int m_PresentWires;
	int m_WireCountsByColor[WIRE_COLOR_MAX];
	bool m_WiresCut[WIRE_COUNT];

	int m_WireToCut;
	bool m_SerialSuffixOdd;

	class WireWatcher {
	private:
		int m_Pin;
		int m_LastValue;

	public:
		void SetPin(int pin) {
			m_Pin = pin;
			m_LastValue = digitalRead(m_Pin);
		}

		bool IsCut() {
			int value = digitalRead(m_Pin);
			bool cut = value == HIGH; //pull up
			m_LastValue = value;
			return cut;
		}
	};

	WireWatcher m_Wires[6];

public:
	WiresModule() {
		SetModuleLedPin(2);
		for (int i = WIRE_0_PIN; i >= WIRE_5_PIN; i--) {
			pinMode(i, INPUT_PULLUP);
		}
	}

	const char* GetName() override {
		return "Wires";
	}

	void Bootstrap() override {
		DefusableModule::Bootstrap();
	}

	ModuleLedDriver* GetModuleLedDriver() override {
		return GetNeopixelLedDriver();
	}

	void Reset() override {
		
	}

	void Standby() override {
		DefusableModule::Standby();
	}

	void Arm() override {
		DefusableModule::Arm();
		for (int i = 0; i < m_PresentWires; i++) {
			m_Wires[i].SetPin(WIRE_0_PIN - m_WireLut[i]);
		}
		memset(m_WiresCut, 0, sizeof(m_WiresCut));
	}

	void ActiveUpdate() override {
		for (int i = 0; i < m_PresentWires; i++) {
			if (!m_WiresCut[i] && m_Wires[i].IsCut()) {
				PRINTF_P("Cut wire %d\n", i);
				if (i == m_WireToCut) {
					Defuse();
				}
				else {
					Strike();
				}
				m_WiresCut[i] = true;
			}
		}
	}

	int GetActualWireIndex(int wireNum) {
		return m_WireLut[wireNum];
	}

	int GetActualWireColor(int wireNum) {
		return m_WireColors[GetActualWireIndex(wireNum)];
	}

	int GetLastWireOfColor(int color) {
		for (int i = m_PresentWires - 1; i >= 0; i--) {
			if (m_WireColors[i] == color) {
				return i;
			}
		}
		return -1;
	}

	int DetermineWireToCut() {
		int FIRST_WIRE = 0;
		int SECOND_WIRE = 1;
		int THIRD_WIRE = 2;
		int FOURTH_WIRE = 3;
		int LAST_WIRE = m_PresentWires - 1;

		switch (m_PresentWires) {
			case 3:
				if (m_WireCountsByColor[WIRE_RED] == 0) return SECOND_WIRE;
				if (m_WireColors[LAST_WIRE] == WIRE_WHITE) return LAST_WIRE;
				if (m_WireCountsByColor[WIRE_BLUE] > 1) {
					return GetLastWireOfColor(WIRE_BLUE);
				}
				return LAST_WIRE;
			case 4:
				if (m_WireCountsByColor[WIRE_RED] > 1 && m_SerialSuffixOdd) {
					return GetLastWireOfColor(WIRE_RED);
				}
				if (m_WireColors[LAST_WIRE] == WIRE_YELLOW && m_WireCountsByColor[WIRE_RED] == 0) return FIRST_WIRE;
				if (m_WireColors[WIRE_BLUE] == 1) return FIRST_WIRE;
				if (m_WireColors[WIRE_YELLOW] > 1) return LAST_WIRE;
				return SECOND_WIRE;
			case 5:
				if (m_WireColors[LAST_WIRE] == WIRE_BLACK && m_SerialSuffixOdd) return FOURTH_WIRE;
				if (m_WireCountsByColor[WIRE_RED] == 1 && m_WireCountsByColor[WIRE_YELLOW] > 1) return FIRST_WIRE;
				if (m_WireCountsByColor[WIRE_BLACK] == 0) return SECOND_WIRE;
				return FIRST_WIRE;
			case 6:
				if (m_WireCountsByColor[WIRE_YELLOW] && m_SerialSuffixOdd) return THIRD_WIRE;
				if (m_WireCountsByColor[WIRE_YELLOW] && m_WireCountsByColor[WIRE_WHITE] > 1) return FOURTH_WIRE;
				if (m_WireCountsByColor[WIRE_RED] == 0) return LAST_WIRE;
				return FOURTH_WIRE;
		}
		return -1;
	}

	void Configure(BombConfig* config) override {
		m_SerialSuffixOdd = config->SerialFlags & BombConfig::LAST_DIGIT_ODD;
	}

	void ConfigureModule(ModuleConfig* config) override {
		m_PresentWires = 0;
		memset(m_WireCountsByColor, 0, sizeof(m_WireCountsByColor));
		for (int i = 0; i < WIRE_COUNT; i++) {
			int color = config->GetEnum(WIRE_VARS[i].Name, WIRE_COLOR_NAMES, WIRE_COLOR_MAX);
			if (color != WIRE_NOT_PRESENT) {
				PRINTF_P("Wire %d color %d\n", m_PresentWires, color);
				m_WireLut[m_PresentWires] = i;
				m_WireColors[m_PresentWires++] = color;
			}
			m_WireCountsByColor[m_WireColors[i]]++;
		}
		m_WireToCut = DetermineWireToCut();
		PRINTF_P("Wire to cut: %d\n", m_WireToCut + 1);
	}

	const InfoStreamBuilderBase::VariableParam* GetVariableInfo() override {
		return WIRE_VARS;
	}

	bconf::SyncFlag GetSyncFlags() {
		return bconf::FETCH_CONFIG;
	}

	bconf::BombEventBit GetAcceptedEvents() override {
		return bconf::NONE_BITS;
	}
};

WiresModule mod;

void setup() {
	ComponentMain::GetInstance()->Setup(&mod);
}

void loop() {
	ComponentMain::GetInstance()->Loop();
}