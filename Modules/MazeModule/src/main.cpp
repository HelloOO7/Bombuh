#include <Arduino.h>

#include "ComponentMain.h"
#include "BombComponent.h"
#include "BombConfig.h"
#include "GameEvent.h"
#include "NeopixelModuleLedDriver.h"
#include "Adafruit_NeoPixel.h"
#include "EnumFlagOperators.h"

enum CellFlag {
	WU = (1 << 0),
	WD = (1 << 1),
	WL = (1 << 2),
	WR = (1 << 3),

	STATE_SHIFT = 4
};

DEFINE_ENUM_FLAG_OPERATORS(CellFlag)

static constexpr int MAZE_DIM = 6;

struct MazePoint {
	uint8_t X;
	uint8_t Y;

	bool Equals(MazePoint& other) {
		return X == other.X && Y == other.Y;
	}

	unsigned int DistX(MazePoint& other) {
		return abs((int)other.X - (int)X);
	}

	unsigned int DistY(MazePoint& other) {
		return abs((int)other.X - (int)X);
	}
};

struct MazeMap {
	uint8_t Cells[MAZE_DIM][MAZE_DIM];

	MazePoint Id1;
	MazePoint Id2;
};

static constexpr PROGMEM MazeMap MAZES[] {
	{
		{
			{0, WD, WR, WL, WD, WD},
			{WR, WL, WR | WD, WL | WD, WU | WD, WU},
			{WR, WL | WD, WU | WR, WL | WU, WU | WD, 0},
			{WR, WL | WU | WD, WD, WD | WR, WR | WU | WD, 0},
			{0, WU | WD, WU | WR, WU | WL, WU | WR | WD, WL},
			{0, WU | WR, WL, WR, WL | WU, 0}
		},
		{0, 1}, {5, 2}
	},
	{
		{
			{WD, 0, WD | WR, WL, 0, WD},
			{WU, WD | WR, WL | WU, WD | WR, WL | WD, WU},
			{WR, WL | WU, WD | WR, WL | WU, WU | WD, 0},
			{0, WD | WR, WL | WU, WD | WR, WL | WU | WR, WL},
			{WR, WR | WU | WL, WL | WR, WL | WU, WD | WR, WL},
			{WR, WL, WR, WL, WU, 0}
		},
		{4, 1}, {1, 3}
	},
	{
		{
			{0, WD, WR, WL | WR, WL, 0},
			{WD | WR, WL | WU | WR, WL | WR, WL | WD, WD | WR, WL},
			{WU, WR, WL | WR, WL | WU, WU | WR, WL},
			{WR, WL | WR, WL | WR, WL | WR, WL | WR, WL},
			{WR, WL | WD, WD | WR, WL | WR, WL | WR, WL},
			{0, WU, WU, WR, WL, 0}
		},
		{3, 3}, {5, 3}
	},
	{
		{
			{0, WR, WL | WD, WD, WD, 0},
			{WR, WL | WR, WL | WU, WU | WD, WU | WD, 0},
			{WR, WL | WD, WD | WR, WL | WU, WU | WR | WD, WL},
			{WR, WL | WU | WD, WU | WD, WD, WU | WD, 0},
			{0, WU | WD, WU | WD, WU | WD, WU | WR, WL},
			{0, WU, WU | WR, WU | WL, WR, WL}
		},
		{0, 0}, {0, 3}
	},
	{
		{
			{WD, WD, WD, WD, 0, 0},
			{WU, WU | WD, WU | WD, WU, WD | WR, WL | WD},
			{0, WU, WL | WU | WD, WD | WR, WL | WU, WU},
			{WR, WD, WD | WU, WU | WR, WL | WD | WU, WL},
			{WR, WL | WU, WU | WD, WD, WD | WU | WR, WL},
			{WR, WL, WU, WU, WU, 0}
		},
		{4, 2}, {3, 5}
	},
	{
		{
			{WR, WL, WR, WL | WD, 0, 0},
			{WR, WL | WR, WL | WR, WL | WU, WD | WR, WL},
			{0, WD | WR, WL | WD | WR, WL | WR, WL | WU, WD},
			{WD, WU, WL | WU, WR, WL | WL, WL | WU},
			{WU, WD | WR, WL | WD | WR, WL | WR, WL | WD, 0},
			{0, WU, WU, WR, WL | WU, 0}
		},
		{4, 0}, {2, 4}
	},
	{
		{
			{0, WD, WD, WR, WL, 0},
			{WR, WL | WU, WU | WD | WR, WL | WD, WD | WR, WL},
			{WD, WD | WR, WL | WU, WU | WR | WD, WL | WU, WD},
			{WU, WU | WR, WL, WU | WD, WD | WR, WL | WU},
			{WR, WL | WD | WR, WL | WD, WD | WU, WU | WR, WL},
			{0, WU, WU, WU, 0, 0}
		},
		{1, 0}, {1, 5}
	},
	{
		{
			{WR, WL, WD, WR, WL, 0},
			{0, WD, WD | WU | WR, WL | WD, WD | WR, WL},
			{WR, WL | WU, WU | WD, WU | WD, WU | WR, WL},
			{WR, WL | WD, WU | WR, WL | WU | WD, WD, WD},
			{WR, WL | WU | WR, WL | WD, WU | WD, WU | WD, WU | WD},
			{0, 0, WU, WU, WU, WU},
		},
		{3, 0}, {2, 3}
	},
	{
		{
			{WR, WL, WD, WD, 0, 0},
			{WR, WL | WR, WL | WU, WU | WD | WR, WL | WR, WL},
			{0, WD, WD | WR, WL | WU, WD | WR, WL},
			{WR, WL | WU | WR, WL | WU, WD | WR, WL | WD | WU, 0},
			{WR, WL | WR, WL | WR, WL | WU, WU | WR, WL | WD},
			{0, WR, WL, WR, WL, WU}
		},
		{2, 1}, {0, 4}
	},
};

class MazeModule : public DefusableModule, NeopixelLedModuleTrait {
private:
	static constexpr int PIN_MAZE_STRIP = 3;
	static constexpr int MAZE_REAL_DIM = 8;
	static constexpr int PIN_BUTTON_U = 6;
	static constexpr int PIN_BUTTON_R = 7;
	static constexpr int PIN_BUTTON_D = 8;
	static constexpr int PIN_BUTTON_L = 9;

	static constexpr uint32_t COLOR_HERO = 0xFFFFFF;
	static constexpr uint32_t COLOR_TREASURE = 0xFF0000;
	static constexpr uint32_t COLOR_IDENT = 0x00FF00;

	struct DirButton {
	public:
		const int XMove;
		const int YMove;
		const CellFlag MoveFlag;
	private:
		static constexpr int DEBOUNCE_INTERVAL = 100;

		int m_Pin;
		int m_LastValue;
		unsigned long long m_LastStateChange;

	public:
		DirButton(int pin, int xmove, int ymove, CellFlag moveFlag) : XMove{xmove}, YMove{ymove}, MoveFlag{moveFlag}, m_Pin{pin}, m_LastStateChange{0} {
			pinMode(m_Pin, INPUT_PULLUP);
			m_LastValue = digitalRead(m_Pin);
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

	Adafruit_NeoPixel m_MazeStrip;
	DirButton m_Buttons[4] {
		DirButton(PIN_BUTTON_U, 0, -1, CellFlag::WU),
		DirButton(PIN_BUTTON_D, 0, 1, CellFlag::WD),
		DirButton(PIN_BUTTON_L, -1, 0, CellFlag::WL),
		DirButton(PIN_BUTTON_R, 1, 0, CellFlag::WR)
	};
	MazeMap m_MazeMap;
	MazePoint m_StartPoint;
	MazePoint m_EndPoint;
	MazePoint m_HeroPos;
public:
	MazeModule() : m_MazeStrip(MAZE_REAL_DIM * MAZE_REAL_DIM, 3, NEO_GRB | NEO_KHZ800) {
		SetModuleLedPin(2);
		pinMode(PIN_MAZE_STRIP, OUTPUT);
	}

	const char* GetName() override {
		return "Maze";
	}

	void Bootstrap() override {
		DefusableModule::Bootstrap();
	}

	ModuleLedDriver* GetModuleLedDriver() override {
		return GetNeopixelLedDriver();
	}

	void Reset() override {
		DefusableModule::Reset();
		m_MazeStrip.clear();
		m_MazeStrip.show();
	}

	void Standby() override {
		DefusableModule::Standby();
	}

	int GetPixelIndex(int x, int y) {
		return MAZE_REAL_DIM * (y + 1) + ((MAZE_REAL_DIM - MAZE_DIM) >> 1) + x;
	}

	void ChangeHeroPos(MazePoint& newPos) {
		MazePoint& point = m_HeroPos;
		uint32_t oldColor = 0x000000;
		if (point.Equals(m_MazeMap.Id1) || point.Equals(m_MazeMap.Id2)) {
			oldColor = COLOR_IDENT;
		}
		m_MazeStrip.setPixelColor(GetPixelIndex(point.X, point.Y), oldColor);
		m_MazeStrip.setPixelColor(GetPixelIndex(newPos.X, newPos.Y), COLOR_HERO);
		m_HeroPos = newPos;
	}

	void InitDispPos(MazePoint& point, uint32_t color) {
		m_MazeStrip.setPixelColor(GetPixelIndex(point.X, point.Y), color);
	}

	void Arm() override {
		DefusableModule::Arm();
		for (int i = 0; i < MAZE_DIM; i++) {
			for (int j = 0; j < MAZE_DIM; j++) {
				m_MazeMap.Cells[i][j] &= 0xF; //reset states
			}
		}
		PRINTF_P("Ident 1: %d:%d\n", m_MazeMap.Id1.X, m_MazeMap.Id1.Y);
		PRINTF_P("Ident 2: %d:%d\n", m_MazeMap.Id2.X, m_MazeMap.Id2.Y);
		m_HeroPos = m_StartPoint;
		InitDispPos(m_MazeMap.Id1, COLOR_IDENT);
		InitDispPos(m_MazeMap.Id2, COLOR_IDENT);
		InitDispPos(m_HeroPos, COLOR_HERO);
		InitDispPos(m_EndPoint, COLOR_TREASURE);
		m_MazeStrip.show();
	}

	void ActiveUpdate() override {
		for (int i = 0; i < 4; i++) {
			DirButton& btn = m_Buttons[i];
			if (btn.IsPressed()) {
				PRINTF_P("Button pressed, move X: %d, Y %d\n", btn.XMove, btn.YMove);
				MazePoint newPos = m_HeroPos;
				newPos.X += btn.XMove;
				newPos.Y += btn.YMove;
				if (newPos.X >= 0 && newPos.Y >= 0 && newPos.X < MAZE_DIM && newPos.Y < MAZE_DIM) {
					uint8_t* pcell = &m_MazeMap.Cells[m_HeroPos.Y][m_HeroPos.X];
					uint8_t cell = *pcell;
					if (cell & btn.MoveFlag) {
						if (!(cell & ((btn.MoveFlag) << STATE_SHIFT))) {
							Strike();
							*pcell |= (btn.MoveFlag << STATE_SHIFT);
							return;
						}
					}
					else {
						ChangeHeroPos(newPos);
						m_MazeStrip.show();
						if (m_HeroPos.Equals(m_EndPoint)) {
							Defuse();
							return;
						}
					}
				}
			}
		}
	}

	void RandomPoint(MazePoint* p) {
		p->X = random(MAZE_DIM);
		p->Y = random(MAZE_DIM);
	}

	bool IdPointConflict(MazePoint& point) {
		return point.Equals(m_MazeMap.Id1) || point.Equals(m_MazeMap.Id2);
	}

	void LoadConfiguration(BombConfig* config) override {
		randomSeed(config->RandomSeed);
		int mazeId = random(sizeof(MAZES) / sizeof(MAZES[0]));
		PRINTF_P("Generated maze ID: %d\n", mazeId);
		memcpy_P(&m_MazeMap, &MAZES[mazeId], sizeof(MAZES[mazeId]));
		RandomPoint(&m_StartPoint);
		while (IdPointConflict(m_StartPoint)) {
			RandomPoint(&m_StartPoint);
		}
		RandomPoint(&m_EndPoint);
		while (
			(m_EndPoint.DistX(m_StartPoint) <= 1 && m_EndPoint.DistY(m_StartPoint) <= 1)
			|| IdPointConflict(m_EndPoint)
			) {
			RandomPoint(&m_EndPoint);
		}
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
		return bconf::NONE_BITS;
	}
};

MazeModule mod;

void setup() {
	ComponentMain::GetInstance()->Setup(&mod);
}

void loop() {
	ComponentMain::GetInstance()->Loop();
}