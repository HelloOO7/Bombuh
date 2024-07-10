#include <Arduino.h>

#define SFX_ENABLED

#include "ComponentMain.h"
#include "BombComponent.h"
#include "BombConfig.h"
#include "GameEvent.h"
#include "NeopixelModuleLedDriver.h"
#include "Adafruit_NeoPixel.h"
#include "EnumFlagOperators.h"

enum SimonColor {
	COLOR_MIN,
	
	COLOR_RED = COLOR_MIN,
	COLOR_BLUE,
	COLOR_GREEN,
	COLOR_YELLOW,

	COLOR_MAX
};

static constexpr int COLOR_LUT_VOWEL[][COLOR_MAX] {
	{COLOR_BLUE, COLOR_RED, COLOR_YELLOW, COLOR_GREEN},
	{COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_RED},
	{COLOR_GREEN, COLOR_RED, COLOR_YELLOW, COLOR_BLUE}
};

static constexpr int COLOR_LUT_NO_VOWEL[][COLOR_MAX] {
	{COLOR_BLUE, COLOR_YELLOW, COLOR_GREEN, COLOR_RED},
	{COLOR_RED, COLOR_BLUE, COLOR_YELLOW, COLOR_GREEN},
	{COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_RED}
};

class SimonModule : public DefusableModule, NeopixelLedModuleTrait, public EventfulComponentTrait<SimonModule> {
private:
	static constexpr int SEQUENCE_MIN_LENGTH = 4;
	static constexpr int SEQUENCE_MAX_LENGTH = 6;

	bool m_SerialVowel;
	
	bool m_HasInteracted;
	int m_Sequence[SEQUENCE_MAX_LENGTH];
	int m_SequenceLength;
	int m_CurrentSequenceLength;
	int m_InputPos;

	class SimonButton {
	public:
		const int m_Color;
	private:
		static constexpr int DEBOUNCE_INTERVAL = 100;

		int m_Pin;
		int m_LedPin;
		int m_LastValue;
		unsigned long long m_LastStateChange;

	public:
		SimonButton(int color, int btnPin, int ledPin) :
		m_Color{color},
		m_Pin{btnPin},
		m_LedPin{ledPin},
		m_LastStateChange{0} {
			pinMode(m_Pin, INPUT_PULLUP);
			pinMode(m_LedPin, OUTPUT);
			TurnOff();
			m_LastValue = digitalRead(m_Pin);
		}

		void TurnOn() {
			digitalWrite(m_LedPin, HIGH);
		}

		void TurnOff() {
			digitalWrite(m_LedPin, LOW);
		}

		bool IsPressed() {
			int val = digitalRead(m_Pin);
			if (val != m_LastValue) {
				unsigned long long time = millis();
				if (!m_LastStateChange || m_LastStateChange + DEBOUNCE_INTERVAL < time) {
					m_LastStateChange = time;
					m_LastValue = val;
					return val == LOW;
				}
			}
			return false;
		}
	};

	SimonButton m_Buttons[COLOR_MAX] {
		SimonButton(COLOR_YELLOW, 10, 9),
		SimonButton(COLOR_GREEN, 8, 7),
		SimonButton(COLOR_RED, 6, 5),
		SimonButton(COLOR_BLUE, 4, 3)
	};

public:
	SimonModule() 
	{
		SetModuleLedPin(2);
	}

	const char* GetName() override {
		return "Simon Says";
	}

	void Bootstrap() override {
		DefusableModule::Bootstrap();
	}

	ModuleLedDriver* GetModuleLedDriver() override {
		return GetNeopixelLedDriver();
	}

	void TurnOffAllButtons() {
		for (int i = 0; i < COLOR_MAX; i++) {
			m_Buttons[i].TurnOff();
		}
	}

	void Reset() override {
		DefusableModule::Reset();
		m_Events.CancelAll();
		TurnOffAllButtons();
		noTone(A3);
	}

	void Standby() override {
		DefusableModule::Standby();
		m_Events.CancelAll();
		TurnOffAllButtons();
	}

	game::EventChainHandle<SimonModule> m_DemoEvents;

	void StartDemoEvent(unsigned long initialDelay) {
		StartEvent(GenerateDemoEvents(initialDelay), &m_DemoEvents);
	}

	void Arm() override {
		DefusableModule::Arm();
		m_CurrentSequenceLength = 1;
		m_InputPos = 0;
		m_HasInteracted = false;
		StartDemoEvent(2000);
	}

	int GetRequestedColor() {
		const int(*lut)[COLOR_MAX] = m_SerialVowel ? COLOR_LUT_VOWEL : COLOR_LUT_NO_VOWEL;
		BOMB_ASSERT(m_InputPos < m_SequenceLength)
		BOMB_ASSERT(m_Sequence[m_InputPos] < COLOR_MAX)
		return lut[min(m_Bomb->GetStrikes(), 2)][m_Sequence[m_InputPos]];
	}

	game::EventChainHandle<SimonModule> m_FlashOffHandles[COLOR_MAX];

	static bool FlashButtonEvent(Event* event, SimonModule* mod, int* data) {
		SimonButton* b = nullptr;
		for (int i = 0; i < COLOR_MAX; i++) {
			if (mod->m_Buttons[i].m_Color == *data) {
				b = &mod->m_Buttons[i];
			}
		}
		if (b) {
			BOMB_ASSERT(b->m_Color < COLOR_MAX)
			mod->m_FlashOffHandles[b->m_Color].Cancel();
			b->TurnOn();
			#ifdef SFX_ENABLED
			if (mod->m_HasInteracted) {
				static int FREQ_TABLE[] {554, 659, 784, 989};

				tone(A3, FREQ_TABLE[b->m_Color], 500);
			}
			#endif
			Event* turnOff = game::CreateWaitEvent<SimonModule>(450);
			turnOff->Then((new Event(function(Event* event, SimonModule* mod, SimonButton* btn) {
				btn->TurnOff();
				return true;
			}, b))->SetDataPermanent());
			mod->StartEvent(turnOff, &mod->m_FlashOffHandles[b->m_Color]);
		}
		return true;
	}

	Event* GenerateDemoEvents(long initialDelay) {
		Event* event = game::CreateWaitEvent<SimonModule>(initialDelay);
		Event* last = event;
		for (int i = 0; i < m_CurrentSequenceLength; i++) {
			last = last
					->Then(new Event(FlashButtonEvent, new int(m_Sequence[i])))
					->Then(game::CreateWaitEvent<SimonModule>(600));
		}
		last->Then(new Event(function(Event* event, SimonModule* mod) {
			event->Then(mod->GenerateDemoEvents(5000));
			return true;
		}));
		return event;
	}

	void ActiveUpdate() override {
		for (int i = 0; i < COLOR_MAX; i++) {
			SimonButton& btn = m_Buttons[i];
			if (btn.IsPressed()) {
				m_HasInteracted = true;
				m_DemoEvents.Cancel();
				if (GetRequestedColor() == btn.m_Color) {
					StartEvent(new Event(FlashButtonEvent, new int(btn.m_Color)));
					m_InputPos++;
					if (m_InputPos == m_CurrentSequenceLength) {
						m_InputPos = 0;
						m_CurrentSequenceLength++;
						if (m_CurrentSequenceLength == m_SequenceLength + 1) {
							Defuse();
							return;
						}
						StartDemoEvent(2000);
					}
					else {
						StartDemoEvent(5000);
					}
				}
				else {
					PRINTF_P("Requested color for %d is %d got %d (serial %d strikes %d)\n", m_Sequence[m_InputPos], GetRequestedColor(), btn.m_Color, m_SerialVowel, m_Bomb->GetStrikes());
					Strike();
					m_InputPos = 0;
					StartDemoEvent(2000);
				}
			}
		}
	}

	void Display() override {
		DefusableModule::Display();
		m_Events.Update();
	}

	void Configure() override {
		m_SequenceLength = random(SEQUENCE_MIN_LENGTH, SEQUENCE_MAX_LENGTH + 1);
		for (int i = 0; i < m_SequenceLength; i++) {
			m_Sequence[i] = random(COLOR_MIN, COLOR_MAX);
		}
	}

	void LoadConfiguration(BombConfig* config) override {
		randomSeed(config->RandomSeed);
		m_SerialVowel = config->SerialFlags & BombConfig::CONTAINS_VOWEL;
		PRINTF_P("Serial number vowel: %d\n", m_SerialVowel);
	}

	void LoadConfiguration(ModuleConfig* config) override {
		
	}

	const InfoStreamBuilderBase::VariableParam* GetVariableInfo() override {
		return BOMB_NO_VARIABLES;
	}

	bconf::SyncFlag GetSyncFlags() {
		return bconf::FETCH_CONFIG;
	}

	bconf::BombEventBit GetAcceptedEvents() override {
		return bconf::STRIKE_BIT;
	}
};

SimonModule mod;

void setup() {
	ComponentMain::GetInstance()->Setup(&mod);
}

void loop() {
	ComponentMain::GetInstance()->Loop();
}