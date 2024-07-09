#ifndef __BOMBCOMPONENT_H
#define __BOMBCOMPONENT_H

class BombComponent;

#include "BombInterface.h"
#include "BombConfig.h"
#include "Common.h"
#include "GameEvent.h"
#include "ModuleLedDriver.h"

extern const InfoStreamBuilderBase::VariableParam __BOMB_NO_VARIABLES[];

#define BOMB_VARIABLES_ARRAY(...) (const InfoStreamBuilderBase::VariableParam[]) {__VA_ARGS__, {nullptr, VAR_NULLTYPE}}
#define BOMB_NO_VARIABLES __BOMB_NO_VARIABLES

class BombComponent {
    friend class BombInterface;
protected:
    BombInterface* m_Bomb;

    BombComponent();

private:
    size_t m_VariableCount;
    bool m_ModuleConfigDone;
    bool m_BombConfigDone;

public:
    virtual ~BombComponent();

    void Init(BombInterface* bomb);

    virtual void LoadConfiguration(void* config);
    virtual void LoadConfiguration(BombConfig* config);

    virtual void Configure();

    void ClearConfiguredFlags();

    virtual bconf::BombEventBit GetAcceptedEvents();

    virtual void Reset();

    virtual const InfoStreamBuilderBase::VariableParam* GetVariableInfo();

    virtual void GetInfo(void** pData, size_t* pSize);

    void BuildVariableInfo(InfoStreamBuilderBase* builder);

    virtual void Bootstrap();
    
    virtual void Standby();

    virtual void Arm();

    virtual void Update();

    virtual void Display();

    virtual void IdleDisplay();

    virtual void OnEvent(uint8_t id, void* data);

    virtual bconf::SyncFlag GetSyncFlags();
};

class NamedComponentTrait {
public:
    virtual const char* GetName();
};

template<typename M>
class EventfulComponentTrait {
protected:
    using Event = game::Event<M>;

    game::EventManager<M> m_Events;

    EventfulComponentTrait() : m_Events(static_cast<M*>(this)) {

    }

    inline void StartEvent(Event* event, game::EventChainHandle<M>* handle = nullptr) {
        m_Events.Start(event, handle);
    }

    inline void CancelAllEvents() {
        m_Events.CancelAll();
    }

    inline void Standby() {
        m_Events.CancelAll();
    }
};

class BombModule : public BombComponent, public NamedComponentTrait {
    
    virtual void LoadConfiguration(ModuleConfig* config) = 0;
    void LoadConfiguration(void* config) override; 

    virtual BombConfig::ModuleFlag GetModuleFlags() = 0;

    void GetInfo(void** pData, size_t* pSize) override;
};

class DefusableModule : public BombModule {
private:
    bool m_IsDefused;

    game::EventManager<ModuleLedDriver>* m_LightEvents;
    game::EventQueue<ModuleLedDriver>* m_LightEventQueue;

    game::EventQueue<ModuleLedDriver>::Mutex m_LightMutex;
public:
    BombConfig::ModuleFlag GetModuleFlags() override;

protected:
    void Defuse();
    void Strike();

    virtual ModuleLedDriver* GetModuleLedDriver() = 0;

    void TurnOffLed();
public:
    virtual void Bootstrap() override;
    virtual void Standby() override;
    virtual void Reset() override;
    virtual void Arm() override;
    virtual void OnEvent(uint8_t id, void* data) override;

    void Update() override;
    void Display() override;
    void IdleDisplay() override;
    virtual void ActiveUpdate();
};

class BombPort : public BombComponent, NamedComponentTrait {

    virtual void LoadConfiguration(PortConfig* config) = 0;
    void LoadConfiguration(void* config) override;

    void GetInfo(void** pData, size_t* pSize) override;
};

class BombLabel : public BombComponent {

    virtual const char** GetTextOptions() = 0;

    virtual void LoadConfiguration(LabelConfig* config) = 0;
    void LoadConfiguration(void* config) override;

    void GetInfo(void** pData, size_t* pSize) override;
};

class BombBattery : public BombComponent {

    virtual uint8_t GetBatteryCount() = 0;
    virtual uint8_t GetBatterySize() = 0;

    virtual void LoadConfiguration(BatteryConfig* config) = 0;
    void LoadConfiguration(void* config) override;

    void GetInfo(void** pData, size_t* pSize) override;
};

#endif