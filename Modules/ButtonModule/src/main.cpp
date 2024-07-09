#include <Arduino.h>

#include "ComponentMain.h"
#include "BombComponent.h"
#include "BombConfig.h"
#include "GameEvent.h"
#include "NeopixelModuleLedDriver.h"
#include "Adafruit_NeoPixel.h"
#include "EnumFlagOperators.h"
#include "lambda.h"

enum ButtonColorEnum {
	COLOR_ANY,
	COLOR_BLUE,
	COLOR_WHITE,
	COLOR_YELLOW,
	COLOR_RED,

	COLOR_MAX
};

static constexpr const char* COLOR_NAMES[] = {
	"",
	"BLUE",
	"WHITE",
	"YELLOW",
	"RED"
};

enum ButtonLabelEnum {
	LABEL_ANY,
	LABEL_ABORT,
	LABEL_DETONATE,
	LABEL_HOLD,

	LABEL_MAX
};

static constexpr const char* BUTTON_LABELS[] = {
	"",
	"ABORT",
	"DETONATE",
	"HOLD"
};

struct StripColor {
	uint8_t TimerDigit;
	uint32_t RGB;
};

static constexpr const StripColor STRIP_COLORS[] {
	{4, 0x0000FF},
	{1, 0xFFFFFF},
	{5, 0xFFEE00},
	{1, 0xFF0000}
};

class ButtonModule : public DefusableModule, NeopixelLedModuleTrait, public EventfulComponentTrait<ButtonModule> {
private:
	static constexpr int HOLD_THRESHOLD = 500;
	static constexpr int RELEASE_DIGIT_PRESS = -1;
	static constexpr int BUTTON_PIN = 4;

	enum class ButtonInteraction {
		PRESS,
		HOLD
	};

	enum class ButtonResponse {
		NONE,
		PRESS,
		RELEASE
	};

	struct HugeButton {
	private:
		static constexpr int DEBOUNCE_INTERVAL = 100;

		int m_Pin;
		int m_LastValue;
		unsigned long long m_LastStateChange;

	public:
		HugeButton(int pin) : m_Pin{pin}, m_LastStateChange{0} {
			pinMode(m_Pin, INPUT_PULLUP);
			m_LastValue = digitalRead(m_Pin);
		}

		ButtonResponse GetStateChange() {
			int val = digitalRead(m_Pin);
			if (val != m_LastValue) {
				unsigned long long time = millis();
				if (!m_LastStateChange || m_LastStateChange + DEBOUNCE_INTERVAL < time) {
					m_LastStateChange = time;
					m_LastValue = val;
					return (val == HIGH) ? ButtonResponse::PRESS : ButtonResponse::RELEASE;
				}
			}
			return ButtonResponse::NONE;
		}
	};

	int m_Label;
	int m_Color;
	int m_BatteryCount;
	bool m_IsCAR;
	bool m_IsFRK;
	uint32_t m_StripSeed;

	ButtonInteraction m_Interaction;
	int m_ReleaseDigit;

	HugeButton m_Button;
	unsigned long m_HoldStart;
	bool m_IsHolding;

	Adafruit_NeoPixel m_LedStrip;

public:
	ButtonModule() : m_Button(BUTTON_PIN), m_LedStrip(2, 8, NEO_GRB | NEO_KHZ800) {
		SetModuleLedPin(2);
		m_LedStrip.begin();
	}

	const char* GetName() override {
		return "Button";
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
		EventfulComponentTrait::Standby();
		m_LedStrip.clear();
		m_LedStrip.show();
	}

	void Arm() override {
		DefusableModule::Arm();
		m_HoldStart = 0;
		m_IsHolding = false;
		randomSeed(m_StripSeed);
	}

	void SetStripColor(uint32_t rgb) {
		m_LedStrip.fill(rgb);
		m_LedStrip.show();
	}

	void Interact(ButtonInteraction action) {
		
	}

	union ARGB {
		uint32_t ARGB;
		struct {
			uint8_t B;
			uint8_t G;
			uint8_t R;
			uint8_t A;
		};
	};

	struct FadeEventParam {
		ARGB SrcColor;
		ARGB DstColor;
		unsigned long StartTime;
		unsigned long Duration;
	};

	static FadeEventParam* CreateStripAnimation(uint32_t srcColor, uint32_t dstColor, unsigned long length) {
		return new FadeEventParam{srcColor, dstColor, 0, length};
	}

	inline uint8_t LerpChannel(uint8_t l, uint8_t r, float weight) {
		return l + (int16_t)(((int16_t)r - (int16_t)l) * weight);
	}

	bool AnimateLedStrip(FadeEventParam* param) {
		unsigned long time = millis();
		if (time > param->StartTime + param->Duration) {
			return true;
		}
		float weight = -((float)cosf(PI / param->Duration * (millis() - param->StartTime)) - 1.0f) * 0.5f;
		ARGB color;
		color.A = 0;
		color.R = LerpChannel(param->SrcColor.R, param->DstColor.R, weight);
		color.G = LerpChannel(param->SrcColor.G, param->DstColor.G, weight);
		color.B = LerpChannel(param->SrcColor.B, param->DstColor.B, weight);
		SetStripColor(color.ARGB);
		return false;
	}

	static bool PulseLoopEvent(Event* event, ButtonModule* mod, FadeEventParam* param) {
		if (!param->StartTime) {
			param->StartTime = millis();
		}
		if (mod->AnimateLedStrip(param)) {
			FadeEventParam* pulseBack = mod->CreateStripAnimation(param->DstColor.ARGB, param->SrcColor.ARGB, param->Duration);
			event->Then(new Event(PulseLoopEvent, pulseBack));
			return true;
		}
		return false;
	}

	static bool AnimateEvent(Event* event, ButtonModule* mod, FadeEventParam* param) {
		if (!param->StartTime) {
			param->StartTime = millis();
		}
		return mod->AnimateLedStrip(param);
	}

	static uint32_t DarkenColor(uint32_t color) {
		ARGB argb;
		argb.ARGB = color;
		argb.R >>= 2;
		argb.G >>= 2;
		argb.B >>= 2;
		return argb.ARGB;
	}

	void StartStripAnimation(uint32_t rgb) {
		Event* animation = new Event(AnimateEvent, CreateStripAnimation(0x000000, rgb, 500));
		/*animation->Then(
			new Event(PulseLoopEvent, CreateStripAnimation(rgb, DarkenColor(rgb), 1000))
		);*/
		m_Events.Start(animation);
	}

	void ActiveUpdate() override {
		if (m_HoldStart && millis() - m_HoldStart >= HOLD_THRESHOLD) {
			m_IsHolding = true;
			m_HoldStart = 0;
			const StripColor* colorInfo = &STRIP_COLORS[random(sizeof(STRIP_COLORS) / sizeof(STRIP_COLORS[0]))];
			m_ReleaseDigit = colorInfo->TimerDigit;
			PRINTF_P("The button should be released on digit %d\n", m_ReleaseDigit);
			StartStripAnimation(colorInfo->RGB);
		}
		auto state = m_Button.GetStateChange();
		if (state == ButtonResponse::PRESS) {
			PRINTLN_P("Button down.");
			m_HoldStart = millis();
		}
		else if (state == ButtonResponse::RELEASE) {
			PRINTLN_P("Button up!");
			m_Events.CancelAll();
			SetStripColor(0x000000);
			if (!m_IsHolding) {
				if (m_Interaction == ButtonInteraction::PRESS) {
					m_Events.CancelAll();
					Defuse();
				}
				else {
					Strike();
				}
			}
			else {
				if (m_Interaction == ButtonInteraction::PRESS) {
					Strike();
				}
				else {
					uint8_t digits[BombInterface::TIMER_DIGIT_COUNT];
					m_Bomb->GetTimerDigits(digits);
					char chars[5];
					for (int i = 0; i < 4; i++) {
						chars[i] = digits[i] + '0';
					}
					chars[4] = 0;
					PRINTF_P("Timer digits %s\n", chars);
					bool pass = false;
					for (size_t i = 0; i < sizeof(digits) / sizeof(uint8_t); i++) {
						if (digits[i] == m_ReleaseDigit) {
							pass = true;
							break;
						}
					}
					if (pass) {
						Defuse();
					}
					else {
						Strike();
						PRINTLN_P("Uh oh. Struck!");
					}
				}
			}
			m_HoldStart = 0;
			m_IsHolding = false;
		}
	}

	void Display() override {
		DefusableModule::Display();
		m_Events.Update();
	}

	bool TheButtonSays(ButtonLabelEnum label) {
		return m_Label == label;
	}

	bool TheButtonIs(ButtonColorEnum color) {
		return m_Color == color;
	}

	ButtonInteraction DetermineInteraction() {
		if (TheButtonIs(COLOR_BLUE) && TheButtonSays(LABEL_ABORT)) return ButtonInteraction::HOLD;
		if (m_BatteryCount > 1 && TheButtonSays(LABEL_DETONATE)) return ButtonInteraction::PRESS;
		if (TheButtonIs(COLOR_WHITE) && m_IsCAR) return ButtonInteraction::HOLD;
		if (m_BatteryCount > 2 && m_IsFRK) return ButtonInteraction::PRESS;
		if (TheButtonIs(COLOR_YELLOW)) return ButtonInteraction::HOLD;
		if (TheButtonIs(COLOR_RED) && TheButtonSays(LABEL_HOLD)) return ButtonInteraction::PRESS;
		return ButtonInteraction::HOLD;
	}

	void LoadConfiguration(BombConfig* config) override {
		m_StripSeed = config->RandomSeed;
		m_BatteryCount = config->BatteryCount();
		m_IsCAR = config->IsLabelPresent("CAR");
		m_IsFRK = config->IsLabelPresent("FRK");
	}

	void LoadConfiguration(ModuleConfig* config) override {
		m_Label = config->GetEnum("Label", BUTTON_LABELS, LABEL_MAX);
		m_Color = config->GetEnum("Color", COLOR_NAMES, COLOR_MAX);
	}

	void Configure() override {
		m_Interaction = DetermineInteraction();
		PRINTF_P("Configured. The button should be %s.\n", m_Interaction == ButtonInteraction::PRESS ? "pressed" : "held");
	}

	const InfoStreamBuilderBase::VariableParam* GetVariableInfo() override {
		//skip empty fallback enums
		static const auto labels = InfoStreamBuilderBase::EnumExtra{LABEL_MAX - 1, BUTTON_LABELS + 1};
		static const auto colors = InfoStreamBuilderBase::EnumExtra{COLOR_MAX - 1, COLOR_NAMES + 1};

		return BOMB_VARIABLES_ARRAY(
			{"Label", VAR_STR_ENUM, &labels},
			{"Color", VAR_STR_ENUM, &colors}
		);
	}

	bconf::SyncFlag GetSyncFlags() {
		return bconf::FETCH_CONFIG;
	}

	bconf::BombEventBit GetAcceptedEvents() override {
		return bconf::TIMER_SYNC_BIT;
	}
};

ButtonModule mod;

void setup() {
	ComponentMain::GetInstance()->Setup(&mod);
}

void loop() {
	ComponentMain::GetInstance()->Loop();
}