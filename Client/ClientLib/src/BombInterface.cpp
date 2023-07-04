#include "BombClient.h"
#include "BombConfig.h"
#include "AddressObtainer.h"
#include "EnumFlagOperators.h"
#include "BombComponent.h"
#include "Common.h"
#include "BombInterface.h"

BombInterface::BombInterface(BombClient* client, bconf::SyncFlag syncFlags) 
: m_SyncFlags{syncFlags}, m_Client{client} {
    client->AddEventDispatcher(function(uint8_t eventId, void* eventData, BombInterface* iface) {
        iface->OnEvent(eventId, eventData);
    }, this);
}

void BombInterface::OnEvent(uint8_t eventId, void* eventData) {
    switch (eventId) {
        case bconf::RESET:
            m_Client->DiscardRequests();
            break;
        case bconf::STRIKE:
            if (m_SyncFlags & bconf::SYNC_STRIKES) {
                SyncStrikes();
            }
            break;
    }
}

void BombInterface::LoadBombConfig(BombComponent* module) {
    bprotocol::ConfigRequest req;
    m_Client->QueueRequest("GetBombConfig", &req, function(bprotocol::ConfigResponse* resp, BombComponent* module) {
        void* buffer = malloc(resp->m_BufferSize);
        memcpy(buffer, resp->m_Buffer, resp->m_BufferSize);
        BombConfig* conf = BombConfig::FromBuffer(buffer);
        module->Configure(conf);
        free(buffer);
        module->m_BombConfigDone = true;
        module->m_Bomb->AckReadyIfModuleConfigured(module);
    }, module);
}

bool BombInterface::IsAllSyncDone() {
    return m_Client->IsAllSyncDone();
}

void BombInterface::SyncGameClock() {
    bprotocol::SimpleRequest<bombclock_t> req;
    m_Client->QueueRequest("GetClock", &req, function(bombclock_t* resp, BombInterface* iface) {
        iface->UpdateClockValue(*resp - CLOCK_SYNC_CORRECTION);
    }, this);
}

void BombInterface::SyncStrikes() {
    bprotocol::SimpleRequest<uint8_t> req;
    m_Client->QueueRequest("GetStrikes", &req, function(uint8_t* resp, BombInterface* iface) {
        iface->m_State.Strikes = *resp;
    }, this);
}

void BombInterface::LoadComponentConfig(BombComponent* component) {
    bprotocol::ConfigRequest req;
    m_Client->QueueRequest("GetComponentConfigByBusAddress", &req, function(bprotocol::ConfigResponse* resp, BombComponent* component) {
        void* buffer = malloc(resp->m_BufferSize);
        memcpy(buffer, resp->m_Buffer, resp->m_BufferSize);
        component->Configure(buffer);
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
        AckReady();
    }
}

void BombInterface::Strike() {
    m_Client->QueueRequest("AddStrike");
}

void BombInterface::DefuseMe() {
    m_Client->QueueRequest("DefuseComponent");
}

void BombInterface::UpdateClockValue(bombclock_t clock) {
    if (clock < 0) {
        clock = 0;
    }
    m_State.Clock = clock;
}