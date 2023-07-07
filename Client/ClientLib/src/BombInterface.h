#ifndef __BOMBINTERFACE_H
#define __BOMBINTERFACE_H

#include "EnumFlagOperators.h"

class BombInterface;

struct BombState;

namespace bconf {
    enum SyncFlag {
        SYNC_NOTHING,
        FETCH_CONFIG = (1 << 0),
    };

    enum BombEvent {
        RESET,
        CONFIGURE,
        ARM,
        STRIKE,
        EXPLOSION,
        DEFUSAL,
        LIGHTS_OUT,
        LIGHTS_ON,
        TIMER_TICK,
        TIMER_SYNC, //like TIMER_TICK but less frequent. TIMER_TICK fires every second (!)
    };

    enum BombEventBit {
        NONE_BITS = 0,
        RESET_BIT = (1 << RESET),
        CONFIGURE_BIT = (1 << CONFIGURE),
        ARM_BIT = (1 << ARM),
        STRIKE_BIT = (1 << STRIKE),
        EXPLOSION_BIT = (1 << EXPLOSION),
        DEFUSAL_BIT = (1 << DEFUSAL),
        LIGHTS_OUT_BIT = (1 << LIGHTS_OUT),
        LIGHTS_ON_BIT = (1 << LIGHTS_ON),
        TIMER_TICK_BIT = (1 << TIMER_TICK),
        TIMER_SYNC_BIT = (1 << TIMER_SYNC),

        LIGHTS_BITS = LIGHTS_OUT_BIT | LIGHTS_ON_BIT,

        ALWAYS_LISTEN_BITS = RESET_BIT | CONFIGURE_BIT | ARM_BIT | EXPLOSION_BIT | DEFUSAL_BIT
    };

    DEFINE_ENUM_FLAG_OPERATORS(BombEventBit)

    DEFINE_ENUM_FLAG_OPERATORS(SyncFlag)
}

#include "BombClient.h"
#include "BombConfig.h"
#include "AddressObtainer.h"
#include "BombComponent.h"
#include "Common.h"

namespace bprotocol {
    struct ConfigResponse : BombClient::TResponse {
        size_t m_BufferSize;
        char m_Buffer[1];
    };

    struct ConfigRequest : BombClient::TRequest<ConfigResponse> {

    };

    template<typename T>
    struct SimpleRequest : BombClient::TRequest<T> {
    };
}

struct BombState {
    bombclock_t Clock;
    uint8_t     Strikes;
    uint8_t     DefusedModules;

    inline int GetSeconds() {
        return (Clock / 1000) % 60;
    }

    inline int GetHundredths() {
        return (Clock / 10) % 100;
    }

    inline int GetMinutes() {
        return (Clock / 60000);
    }
};

class BombInterface {
private:
    static constexpr int CLOCK_SYNC_CORRECTION = 20; //20ms

    bconf::SyncFlag  m_SyncFlags;

    BombClient*     m_Client;
    BombState       m_State;

public:
    BombInterface(BombClient* client, bconf::SyncFlag syncFlags = bconf::SYNC_NOTHING);

    void OnEvent(uint8_t eventId, void* eventData);

    bool IsAllSyncDone();

    void SyncGameClock();

    void SyncStrikes();

    void LoadBombConfig(BombComponent* module);
    void LoadComponentConfig(BombComponent* module);

    void AckReady();
    void AckReadyIfModuleConfigured(BombComponent* mod);

    void Strike();
    void DefuseMe();

    void UpdateClockValue(bombclock_t clock);

    bool IsAboutToExplode();
};

#endif