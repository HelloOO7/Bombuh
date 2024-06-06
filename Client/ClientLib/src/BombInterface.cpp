#include "BombClient.h"
#include "BombConfig.h"
#include "AddressObtainer.h"
#include "EnumFlagOperators.h"
#include "BombComponent.h"
#include "Common.h"
#include "BombInterface.h"
#include <string.h>
#include <alloca.h>

BombState::BombState() : Clock{0}, Timescale{1.0f}, ClockSyncTime{0}, Strikes{0} {
    
}

BombInterface::BombInterface(BombClient* client, bconf::SyncFlag syncFlags) 
: m_SyncFlags{syncFlags}, m_Client{client} {
    client->AddEventDispatcher(function(uint8_t eventId, void* eventData, BombInterface* iface) {
        iface->OnEvent(eventId, eventData);
    }, this);
}

void BombInterface::OnEvent(uint8_t eventId, void* eventData) {
    switch (eventId) {
        case bconf::RESET:
            m_State = BombState();
            m_Client->DiscardRequests();
            break;
        case bconf::STRIKE:
            SyncStrikes();
            break;
        case bconf::TIMER_SYNC:
            SyncGameClock();
            break;
    }
}

void BombInterface::LoadBombConfig(BombComponent* module) {
    bprotocol::ConfigRequest req;
    m_Client->QueueRequest("GetBombConfig", &req, function(bprotocol::ConfigResponse* resp, BombComponent* module) {
        void* buffer = malloc(resp->m_BufferSize);
        memcpy(buffer, resp->m_Buffer, resp->m_BufferSize);
        BombConfig* conf = BombConfig::FromBuffer(buffer);
        module->LoadConfiguration(conf);
        free(buffer);
        module->m_BombConfigDone = true;
        module->m_Bomb->AckReadyIfModuleConfigured(module);
    }, module);
}

bool BombInterface::IsAllSyncDone() {
    return m_Client->IsAllSyncDone();
}

void BombInterface::SyncGameClock() {
    bprotocol::SimpleRequest<bprotocol::ClockResponse> req;
    m_Client->QueueRequest("GetClock", &req, function(bprotocol::ClockResponse* resp, BombInterface* iface) {
        iface->m_State.ClockSyncTime = millis();
        iface->UpdateClockValue(resp->m_Clock - CLOCK_SYNC_CORRECTION);
        iface->m_State.Timescale = resp->m_Timescale;
    }, this);
}

bombclock_t BombInterface::GetBombTime() {
    bombclock_t val = m_State.Clock - (bombclock_t)((millis() - m_State.ClockSyncTime) * m_State.Timescale);
    if (val < 0) {
        val = 0;
    }
    return val;
}

void BombInterface::GetTimerDigits(uint8_t* dest) {
    bombclock_t time = GetBombTime();
    uint8_t major;
    uint8_t minor;
    if (time < 60L * 1000L) {
        bombclock_t seconds = time / 1000;
        uint8_t hundredths = (time / 10) % 100;
        major = seconds;
        minor = hundredths;
    }
    else {
        bombclock_t seconds = time / 1000;
        uint8_t mm = seconds / 60;
        if (mm > 99) {
            mm = 99;
        }
        uint8_t ss = seconds % 60;
        major = mm;
        minor = ss;
    }
    dest[0] = major / 10;
    dest[1] = major % 10;
    dest[2] = minor / 10;
    dest[3] = minor % 10;
}

void BombInterface::SyncStrikes() {
    bprotocol::SimpleRequest<uint8_t> req;
    m_Client->QueueRequest("GetStrikes", &req, function(uint8_t* resp, BombInterface* iface) {
        iface->m_State.Strikes = *resp;
    }, this);
}

int BombInterface::GetStrikes() {
    return m_State.Strikes;
}

void BombInterface::LoadComponentConfig(BombComponent* component) {
    bprotocol::ConfigRequest req;
    m_Client->QueueRequest("GetComponentConfigByBusAddress", &req, function(bprotocol::ConfigResponse* resp, BombComponent* component) {
        void* buffer = malloc(resp->m_BufferSize);
        memcpy(buffer, resp->m_Buffer, resp->m_BufferSize);
        component->LoadConfiguration(buffer);
        #ifdef DEBUG
        Serial.print("Component config bytes after relocation:");
        for (size_t i = 0; i < resp->m_BufferSize; i++) {
            Serial.print(*(((uint8_t*)buffer) + i), HEX);
            Serial.print(",");
        }
        Serial.println();
        #endif
        free(buffer);
        component->m_ModuleConfigDone = true;
        component->m_Bomb->AckReadyIfModuleConfigured(component);
    }, component);
}

void BombInterface::AckReady() {
    m_Client->QueueRequest("AckReadyToArm");
}

void BombInterface::AckReadyIfModuleConfigured(BombComponent* mod) {
    if (mod->m_BombConfigDone && mod->m_ModuleConfigDone) {
        PRINTLN_P("ModuleConfigured - AckReady!");
        AckReady();
        mod->Configure();
        PRINTLN_P("AckedReady!");
    }
}

void BombInterface::Strike() {
    m_Client->QueueRequest("AddStrike");
}

void BombInterface::DefuseMe() {
    m_Client->QueueRequest("DefuseComponent");
}

void BombInterface::SendServerMessage(bprotocol::ServerMessageType type, const char* text, bool progMem) {
    size_t len = progMem ? strlen_P(text) : strlen(text);
    size_t reqSize = 3 + len;
    bprotocol::ServerMessageRequest* req = (bprotocol::ServerMessageRequest*) alloca(reqSize);
    req->m_Type = type;
    req->m_Length = len;
    progMem ? memcpy_P(req->m_Text, text, len) : memcpy(req->m_Text, text, len);
    m_Client->QueueRequest("OutputDebugMessage", req, reqSize);
}

void BombInterface::UpdateClockValue(bombclock_t clock) {
    if (clock < 0) {
        clock = 0;
    }
    m_State.Clock = clock;
}