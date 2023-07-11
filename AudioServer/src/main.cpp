#include <Arduino.h>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMultiWAV.h"
#include "FS.h"

void InitSDFS() {
	SPI.begin(2, 10, 3, 7);
	SD.begin(7, SPI, 4000000ul, "/sdmc", 16);
}

struct SoundList {
	int Count;
	const char** Paths;
};

int CountFilesInDir(File& dir) {
	int count = 0;
	File file;
	while (file = dir.openNextFile()) {
		count++;
		file.close();
	}
	dir.rewindDirectory();
	return count;
}

void LoadSoundListFromFolder(SoundList* list, const char* path) {
	auto dir = SD.open(path);

	size_t count = CountFilesInDir(dir);	

	list->Count = count;
	list->Paths = new const char*[count];

	int i = 0;
	File file;
	while (file = dir.openNextFile()) {
		list->Paths[i++] = file.path();
		file.close();
	}

	dir.close();
}

SoundList* RandomSoundList() {
	File basedir = SD.open("/Gameplay");

	int cnt = CountFilesInDir(basedir);
	int selected = random(cnt);

	SoundList* ret = nullptr;

	int i = 0;
	File file;
	while (file = basedir.openNextFile()) {
		const char* path = file.path();
		file.close();
		if (i == selected) {
			ret = new SoundList();
			LoadSoundListFromFolder(ret, path);
			break;
		}
	}

	basedir.close();

	return ret;
}

static constexpr const char* WAV_NAMES[] {
	"/Gameplay/GameRoomA/GameRoomA_1.wav",
	"/Gameplay/GameRoomA/GameRoomA_2.wav",
	"/Gameplay/GameRoomA/GameRoomA_3.wav",
	"/Gameplay/GameRoomA/GameRoomA_4.wav",
	"/Gameplay/GameRoomA/GameRoomA_5.wav",
	"/Gameplay/GameRoomA/GameRoomA_6.wav",
	"/Gameplay/GameRoomA/GameRoomA_7.wav",
	"/Gameplay/GameRoomA/GameRoomA_8.wav",
};

AudioOutputI2S* out;
AudioGeneratorMultiWAV::FileHandle* m_AudioHandles[8];
int m_AudIndex = 0;
AudioGeneratorMultiWAV* aud;

void LoadSounds() {
	aud = new AudioGeneratorMultiWAV();
	for (int i = 0; i < 8; i++) {
		m_AudioHandles[i] = aud->prepareFile(new AudioFileSourceSD(WAV_NAMES[i]));
	}
}

void setup() {
	Serial.begin(115200);
  	Serial.println("Hello ESP32C3!!");
	InitSDFS();
	out = new AudioOutputI2S();
	out->SetPinout(19, 18, 6);
	LoadSounds();
	aud->setNextFileCallback([]() {
		AudioGeneratorMultiWAV::FileHandle* file = m_AudioHandles[m_AudIndex++];
		if (m_AudIndex == 8) {
			m_AudIndex = 7;
		}
		return file;
	});
	aud->begin(out);
	Serial.println("Playback started.");

	SoundList* sndlist = RandomSoundList();
	for (int i = 0; i < sndlist->Count; i++) {
		Serial.println(sndlist->Paths[i]);
	}
}

void loopMulti() {
	aud->loop();
}

void loop() {
	loopMulti();
}