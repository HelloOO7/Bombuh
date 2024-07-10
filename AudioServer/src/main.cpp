#include <Arduino.h>

#include "AudioFileSourceSD.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMultiWAV.h"
#include "AudioOutputMixer.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include "WiFi.h"
#include "HTTPClient.h"
#include "ArduinoJson.h"
#include <mutex>
#include <set>

class AudioOutputMixerFix : public AudioOutputMixer {
public:
	bool m_LoopRequested;

	AudioOutputMixerFix(int samples, AudioOutput *sink) : AudioOutputMixer(samples, sink), m_LoopRequested{false} {

	}

	bool loop() override {
		if (m_LoopRequested) {
			m_LoopRequested = false;
			return AudioOutputMixer::loop();
		}
		return false;
	}
};

struct SoundList {
	std::vector<std::string> m_Paths;
};

class MusicController {
protected:
	float m_Gain{1.0f};

public:
	virtual ~MusicController() {

	}

	virtual void Start(AudioOutputMixer* output) = 0;
	virtual bool Loop() = 0;

	virtual void OnSetGain(float gain);

	inline void SetGain(float gain) {
		m_Gain = gain;
		OnSetGain(gain);
	}
};

class AudioFileSourceSDPersistent : public AudioFileSourceSD {
private:
	bool m_AllowClose{false};

public:
	using AudioFileSourceSD::AudioFileSourceSD;

	~AudioFileSourceSDPersistent() {
		AllowClose();
		close();
	}

	bool close() override {
		if (m_AllowClose) {
			return AudioFileSourceSD::close();
		}
		return false;
	}

	void AllowClose() {
		m_AllowClose = true;
	}
};

class SingleMusicController : public MusicController {
private:
	AudioFileSourceSDPersistent m_Source;
	AudioGeneratorWAV m_Generator;
	AudioOutputMixerStub* m_Out;

public:
	SingleMusicController(const char* path) : m_Source(path) {
		m_Out = nullptr;
	}

	~SingleMusicController() {
		m_Source.AllowClose();
		Stop();
	}

	void Stop() {
		if (m_Out) {
			delete m_Out;
			m_Out = nullptr;
		}
		m_Generator.stop();
	}

	void Reset() {
		Stop();
		m_Source.seek(0, SEEK_SET);
	}

	void Start(AudioOutputMixer* output) override {
		if (!m_Out) {
			m_Out = output->NewInput();
			m_Out->SetGain(m_Gain);
		}
		m_Generator.begin(&m_Source, m_Out);
	}

	bool Loop() override {
		if (m_Out) {
			bool retval = m_Generator.loop();
			if (!retval) {
				Stop();
			}
			return retval;
		}
		return false;
	}

	void OnSetGain(float gain) override {
		if (m_Out) {
			m_Out->SetGain(gain);
		}
	}
};

class LoopingMusicController : public MusicController {
private:
	AudioGeneratorMultiWAV m_Generator;

	AudioOutputMixerStub* m_Out;

	AudioFileSourceSD* m_IntroFile;
	AudioFileSourceSD* m_LoopFile;
	AudioGeneratorMultiWAV::FileHandle* m_IntroHandle;
	AudioGeneratorMultiWAV::FileHandle* m_LoopHandle;

	bool m_IsLoopStarted{false};

public:
	LoopingMusicController(const char* introPath, const char* loopPath) {
		if (introPath) {
			m_IntroHandle = m_Generator.prepareFile(m_IntroFile = new AudioFileSourceSD(introPath));
		}
		else {
			m_IntroFile = nullptr;
			m_IntroHandle = nullptr;
			m_IsLoopStarted = true;
		}
		m_LoopHandle = m_Generator.prepareFile(m_LoopFile = new AudioFileSourceSD(loopPath));
		m_Generator.setUserData(this);
		m_Generator.setNextFileCallback([](AudioGeneratorMultiWAV* ag) {
			LoopingMusicController* ctl = static_cast<LoopingMusicController*>(ag->getUserData());
			if (ctl->m_IsLoopStarted) {
				return ctl->m_LoopHandle;
			}
			else {
				ctl->m_IsLoopStarted = true;
				return ctl->m_IntroHandle;
			}
		});
		m_Out = nullptr;
	}

	LoopingMusicController(const char* loopPath) : LoopingMusicController(nullptr, loopPath) {

	}

	~LoopingMusicController() {
		m_Generator.stop();
		delete m_IntroFile;
		delete m_LoopFile;
		delete m_Out;
	}

	void Start(AudioOutputMixer* output) override {
		m_Out = output->NewInput();
		m_Out->SetGain(m_Gain);
		m_Generator.begin(m_Out);
	}

	bool Loop() override {
		return m_Generator.loop();
	}

	void OnSetGain(float gain) override {
		if (m_Out) {
			m_Out->SetGain(gain);
		}
	}
};

class DynamicMusicController : public MusicController {
private:
	AudioGeneratorMultiWAV m_Generator;

	AudioOutputMixerStub* m_Out;

	std::vector<AudioFileSourceSD*> m_Files;
	std::vector<AudioGeneratorMultiWAV::FileHandle*> m_FileHandles;
	
	int m_TrackIndex{0};

	std::function<int()> m_NextTrackFunc;

public:
	DynamicMusicController(SoundList* tracks) {
		m_Files.reserve(tracks->m_Paths.size());
		m_FileHandles.reserve(tracks->m_Paths.size());
		m_Generator.SetBufferSize(4096);
		for (const auto& track : tracks->m_Paths) {
			AudioFileSourceSD* src = new AudioFileSourceSD(track.c_str());
			m_Files.push_back(src);
			m_FileHandles.push_back(m_Generator.prepareFile(src));
		}
		m_Generator.setUserData(this);
		m_Generator.setNextFileCallback([](AudioGeneratorMultiWAV* ag) {
			DynamicMusicController* ctl = static_cast<DynamicMusicController*>(ag->getUserData());
			if (!ctl->GetTrackCount()) {
				return (AudioGeneratorMultiWAV::FileHandle*)nullptr;
			}
			if (ctl->m_NextTrackFunc) {
				ctl->SetTrackIndex(ctl->m_NextTrackFunc());
			}
			return ctl->m_FileHandles[ctl->m_TrackIndex];
		});
		m_Out = nullptr;
	}

	~DynamicMusicController() {
		m_Generator.stop();
		for (const auto& file : m_Files) {
			delete file;
		}
		delete m_Out;
	}

	void ForcePlayTrack(int index) {
		m_Generator.forceChangeTrack(m_FileHandles[index]);
	}

	void SetNextTrackFunc(std::function<int()> func) {
		m_NextTrackFunc = func;
	}

	void SetTrackIndex(int index) {
		m_TrackIndex = index;
	}

	int GetTrackCount() {
		return m_Files.size();
	}

	void Start(AudioOutputMixer* output) override {
		m_Out = output->NewInput();
		m_Out->SetGain(m_Gain);
		m_Generator.begin(m_Out);
	}

	bool Loop() override {
		return m_Generator.loop();
	}

	void OnSetGain(float gain) override {
		if (m_Out) {
			m_Out->SetGain(gain);
		}
	}
};

class MusicPlayer {
public:
	bool m_ReqStop;

	MusicPlayer() {
		m_ReqStop = false;
	}

	virtual ~MusicPlayer() {
		
	}

	virtual void Update() {

	}

	virtual bool Loop() = 0;

	virtual void Start(AudioOutputMixer* out) = 0;

	void Stop() {
		m_ReqStop = true;
	}
};

class SimpleMusicPlayer : public MusicPlayer {
private:
	MusicController* m_Controller;

public:
	SimpleMusicPlayer(const char* path, bool isLooping = false) {
		if (isLooping) {
			m_Controller = new LoopingMusicController(path);
		}
		else {
			m_Controller = new SingleMusicController(path);
		}
	}

	SimpleMusicPlayer(const char* introPath, const char* loopPath) : m_Controller{new LoopingMusicController(introPath, loopPath)} {
		
	}

	~SimpleMusicPlayer() {
		delete m_Controller;
	}

	bool Loop() override {
		return m_Controller->Loop();
	}

	void Start(AudioOutputMixer* out) override {
		m_Controller->Start(out);
	}

	void SetGain(float gain) {
		m_Controller->SetGain(gain);
	}
};

class GameplayMusicPlayer : public MusicPlayer {
private:
	DynamicMusicController m_Controller;
	SingleMusicController*  m_StingerController;

	uint32_t m_TimeLimit;

	uint32_t m_LastTimerUpdate;
	int32_t m_LastTimerValue;
	float	m_Timescale;

	bool m_IsStingerPlaying{false};
	bool m_HasStarted{false};
	int32_t m_StingerEnd{0};

	AudioOutputMixer* m_Mixer;

	int m_TrackIndex;
	int32_t m_LastUpdate;

public:
	GameplayMusicPlayer(SoundList* soundList, const char* stingerPath = nullptr) : m_Controller(soundList) {
		if (stingerPath) {
			m_StingerController = new SingleMusicController(stingerPath);
		}
		else {
			m_StingerController = nullptr;
		}
		m_TrackIndex = 0;
		m_Controller.SetTrackIndex(0);
		m_Controller.SetNextTrackFunc([this](){
			if (this->m_IsStingerPlaying) {
				return this->m_TrackIndex; //do not switch track while playing stinger
			}
			int index = this->GetNextTrackIndex();
			int timer = GetTimerValue();
			this->m_TrackIndex = index;
			printf("Next track %d at timer %02d:%02d\n", index, timer / 60000, (timer % 60000) / 1000);
			return index;
		});
	}

	~GameplayMusicPlayer() {
		delete m_StingerController;
	}

	void Start(AudioOutputMixer* out) override {
		m_Mixer = out;
		m_Controller.Start(out);
	}

	bool Loop() override {
		bool res = m_Controller.Loop();
		if (res && m_StingerController && m_IsStingerPlaying) {
			if (!m_StingerController->Loop()) {
				delete m_StingerController;
				m_StingerController = nullptr;
				m_IsStingerPlaying = false;
			}
		}
		return res;
	}

	void Update() override {
		m_LastUpdate = millis();
		if (m_Controller.GetTrackCount() > 0) {
			int32_t timer = GetTimerValue();
			if (timer == 0) {
				printf("Time ran out. Stopping.\n");
				Stop();
			}
			else if (timer < (30 + 7.0f * m_Timescale) * 1000 && HasStinger() && !m_IsStingerPlaying) {
				printf("Playing stinger.\n");
				m_StingerController->Start(m_Mixer);
				m_IsStingerPlaying = true;
				m_StingerEnd = timer - (int32_t)(6.85f * m_Timescale * 1000);
			}
			int lastTrack = m_Controller.GetTrackCount() - 1;
			if (m_StingerEnd && timer <= m_StingerEnd && m_TrackIndex != lastTrack) {
				m_TrackIndex = lastTrack;
				m_StingerEnd = 0;
				m_Controller.ForcePlayTrack(lastTrack);
			}
		}
	}

	bool HasStinger() {
		return !!m_StingerController;
	}

	void SetStartParams(uint32_t timeLimit, float timescale) {
		m_TimeLimit = timeLimit;
		m_Timescale = timescale;
		UpdateTimer(timeLimit, timescale, true);
	}

	void UpdateTimer(int32_t timerValue, float timescale, bool force = false) {
		if (force || timerValue <= GetTimerValue()) { //prevent lagging back due to request latency
			m_LastTimerUpdate = millis();
			m_LastTimerValue = timerValue;
			m_Timescale = timescale;
		}
	}

	int32_t GetTimerValue() {
		int32_t val = m_LastTimerValue - (int32_t)((millis() - m_LastTimerUpdate) * m_Timescale);
		if (val < 0) {
			val = 0;
		}
		return val;
	}

	int32_t GetPhysicalTimerValue() {
		return GetTimerValue() / m_Timescale;
	}

	int GetNextTrackIndex() {
		if (!m_HasStarted) {
			m_HasStarted = true;
			return 0;
		}
		int32_t timer = GetPhysicalTimerValue();
		if (timer < 30 * 1000) {
			return m_Controller.GetTrackCount() - 1;
		}
		if (timer == m_TimeLimit) {
			return 0;
		}
		int trackCount = m_Controller.GetTrackCount();
		int value = (int)((float)trackCount - ((float)timer / (float)m_TimeLimit) * (float)(trackCount - 1));
		if (value < 0) value = 0;
		if (value > trackCount - 2) value = trackCount - 2;
		return value;
	}
};

void InitSDFS() {
	SPI.begin(2, 10, 3, 7);
	SD.begin(7, SPI, 16000000ul, "/sdmc", 16);
}

int CountSubDirectories(File& dir) {
	int count = 0;
	File file;
	while (file = dir.openNextFile()) {
		if (file.isDirectory()) {
			count++;
		}
		file.close();
	}
	dir.rewindDirectory();
	return count;
}

void LoadSoundListFromFolder(SoundList* list, const char* path) {
	auto dir = SD.open(path);

	int i = 0;
	File file;
	while (file = dir.openNextFile()) {
		list->m_Paths.push_back(std::string(file.path()));
		file.close();
	}

	std::sort(list->m_Paths.begin(), list->m_Paths.end());

	dir.close();
}

std::vector<std::string> g_GameplaySoundDirs;

void PreloadGameplaySoundDirNames() {
	File basedir = SD.open("/Gameplay");

	File file;
	while (file = basedir.openNextFile()) {
		if (file.isDirectory()) {
			g_GameplaySoundDirs.push_back(file.path());
		}
		file.close();
	}

	basedir.close();
}

std::set<std::string> g_AvailableGameplayMusics;

SoundList* RandomGameplaySoundList() {
	if (g_AvailableGameplayMusics.empty()) {
		g_AvailableGameplayMusics.insert(g_GameplaySoundDirs.begin(), g_GameplaySoundDirs.end());
	}

	int selected = random(g_AvailableGameplayMusics.size());
	auto it = g_AvailableGameplayMusics.begin();
	std::advance(it, selected);
	SoundList* list = new SoundList();
	LoadSoundListFromFolder(list, it->c_str());
	g_AvailableGameplayMusics.erase(it);
	return list;
}

SoundList g_ExplosionSoundList;

AudioOutputI2S* g_I2SSink;
AudioOutputMixerFix* g_Mixer;

GameplayMusicPlayer* gameplayMusic{nullptr};
SingleMusicController* g_StrikePlayer{nullptr};
SimpleMusicPlayer* g_LastSetupMusicPlayer;

MusicPlayer* g_Player{nullptr};

AsyncWebServer g_Server(80);
std::mutex g_PlayMutex;

void StopPlayback() {
	g_LastSetupMusicPlayer = nullptr;
	if (g_Player) {
		auto p = g_Player;
		g_Player = nullptr; //ensure m_Player is always invalid in case of context switch
		delete p;
	}
}

void StartPlayback(MusicPlayer* player) {
	StopPlayback();
	g_Player = player;
	player->Start(g_Mixer);
}

void PlayGameplayMusic(int32_t initialTimer) {
	printf("PlayGameplayMusic timer %d\n", initialTimer);
	StopPlayback(); //early call to free file handles
	SoundList* sndlist = RandomGameplaySoundList();
	gameplayMusic = new GameplayMusicPlayer(sndlist, "/Gameplay/Stinger.wav");
	delete sndlist;
	gameplayMusic->SetStartParams(initialTimer, 1.0f);
	StartPlayback(gameplayMusic);
}

void PlayME(const char* name) {
	StopPlayback(); //early call to free file handles
	StartPlayback(new SimpleMusicPlayer(name));
}

void PlaySetupMusic() {
	if (g_Player != nullptr && g_Player == g_LastSetupMusicPlayer) {
		return;
	}
	StopPlayback(); //early call to free file handles
	static const char* const TRACKS[][2] {
		{nullptr, "/Ambient/SetupRoom.wav"},
		{"/Ambient/SetupRoomD_Intro.wav", "/Ambient/SetupRoomD_Loop.wav"},
		{"/Ambient/SetupRoomE_Intro.wav", "/Ambient/SetupRoomE_Loop.wav"}
	};
	const char* const* info = TRACKS[random(sizeof(TRACKS) / sizeof(TRACKS[0]))];
	SimpleMusicPlayer* setupMusic = new SimpleMusicPlayer(info[0], info[1]);
	setupMusic->SetGain(0.4f);
	StartPlayback(setupMusic);
	g_LastSetupMusicPlayer = setupMusic; //musi to byt tady protoze StartPlayback zavola StopPlayback
}

void PlayExplosionSound() {
	StopPlayback();
	if (g_ExplosionSoundList.m_Paths.empty()) {
		return;
	}
	StartPlayback(new SimpleMusicPlayer(g_ExplosionSoundList.m_Paths[random(g_ExplosionSoundList.m_Paths.size())].c_str()));
}

void Pair() {
	HTTPClient cl;
	cl.begin(String("http://") + WiFi.gatewayIP().toString() + "/pair");
	StaticJsonDocument<256> doc;
	JsonArray caps = doc.createNestedArray("device_capabilities");
	caps.add("AudioServer");
	doc["network_address"] = WiFi.localIP().toString();
	uint8_t output[256];
	size_t dataSize = serializeJson(doc, output, sizeof(output));
	cl.setTimeout(5000);
	printf("%d\n", cl.POST(output, dataSize));
	cl.getString();
	cl.end();
}

void EnsureWireless() {
	static bool wifiBegan = false;

	if (WiFi.status() == WL_CONNECTED) {
		return;
	}
	printf("Wifi status %d\n", WiFi.status());
	if (!wifiBegan) {
		wifiBegan = true;
		printf("Begin WiFi connection...\n");
		WiFi.begin("Bombuh", "Julka1234");
	}
	else {
		printf("Reconnecting to WiFi...\n");
		WiFi.reconnect();
	}
	while (WiFi.status() != WL_CONNECTED) {
		if (g_Player) {
			g_Player->Update();
			g_Player->Loop();
			g_Mixer->m_LoopRequested = true;
			g_Mixer->loop();
		}
		delay(1);
	}
	printf("Connected to WiFi! IP: %s\n", WiFi.localIP().toString().c_str());
	Pair();
}

void InitNetworkServer() {
	g_Server.on("/stop", [](AsyncWebServerRequest* request) {
		g_PlayMutex.lock();
		if (g_Player) {
			g_Player->m_ReqStop = true;
		}
		request->send(200);
		g_PlayMutex.unlock();
	});

	g_Server.on("/start-gameplay", [](AsyncWebServerRequest* request) {
		bool post = request->method() == HTTP_POST;
		if (request->hasParam("time_limit", post)) {
			g_PlayMutex.lock();
			PlayGameplayMusic(request->getParam("time_limit", post)->value().toInt());
			request->send(200);
			g_PlayMutex.unlock();
		}
		else {
			request->send(400);
		}
	});

	g_Server.on("/update-gameplay", [](AsyncWebServerRequest* request) {
		unsigned long requestStart = millis();
		bool post = request->method() == HTTP_POST;
		if (request->hasParam("timer", post) && request->hasParam("timescale", post)) {
			g_PlayMutex.lock();
			if (gameplayMusic) {
				gameplayMusic->UpdateTimer(
					request->getParam("timer", post)->value().toInt() - (millis() - requestStart) - 200, 
					request->getParam("timescale", post)->value().toFloat()
				);
				gameplayMusic->Update(); //call update before mutex is unlocked
				request->send(200);
			}
			else {
				request->send(510); //gameplay music not playing
			}
			g_PlayMutex.unlock();
		}
		else {
			request->send(400);
		}
	});

	g_Server.on("/play-victory", [](AsyncWebServerRequest* request) {
		g_PlayMutex.lock();
		PlayME("/ME/GameOver_Fanfare.wav");
		request->send(200);
		g_PlayMutex.unlock();
	});

	g_Server.on("/play-setup", [](AsyncWebServerRequest* request) {
		g_PlayMutex.lock();
		PlaySetupMusic();
		request->send(200);
		g_PlayMutex.unlock();
	});

	g_Server.on("/play-explosion", [](AsyncWebServerRequest* request) {
		g_PlayMutex.lock();
		PlayExplosionSound();
		request->send(200);
		g_PlayMutex.unlock();
	});

	g_Server.on("/play-strike", [](AsyncWebServerRequest* request) {
		g_PlayMutex.lock();
		g_StrikePlayer->Reset();
		g_StrikePlayer->Start(g_Mixer);
		request->send(200);
		g_PlayMutex.unlock();
	});

	g_Server.begin();
}

void setup() {
	Serial.begin(115200);
  	Serial.println("-- Bombuh AudioServer --");
	delay(1000);
	InitSDFS();
	
	g_I2SSink = new AudioOutputI2S();
	g_I2SSink->SetPinout(19, 18, 6);

	PreloadGameplaySoundDirNames();
	g_Mixer = new AudioOutputMixerFix(2048, g_I2SSink);
	LoadSoundListFromFolder(&g_ExplosionSoundList, "/Explosions");
	g_StrikePlayer = new SingleMusicController("/ME/strike.wav");

	WiFi.mode(WIFI_STA);
	InitNetworkServer();

	EnsureWireless();
}

bool g_DoDelay = false;

void loop() {
	g_PlayMutex.lock();
	if (WiFi.status() != WL_CONNECTED) {
		if (g_Player != g_LastSetupMusicPlayer) {
			printf("Stop music - connection lost\n");
			StopPlayback();
		}
		EnsureWireless();
	}

	if (g_StrikePlayer) {
		g_StrikePlayer->Loop();
	}

	if (g_Player) {
		g_Player->Update();
		if (g_Player->m_ReqStop || !g_Player->Loop()) {
			StopPlayback();
		}
	}
	else {
		delay(10); //wait for webserver commands
	}
	g_Mixer->m_LoopRequested = true;
	g_Mixer->loop();
	g_PlayMutex.unlock();
	if (g_DoDelay) {
		delay(1); //spare some time for the web server
	}
	g_DoDelay = !g_DoDelay;
}