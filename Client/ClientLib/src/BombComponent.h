#ifndef __BOMBCOMPONENT_H
#define __BOMBCOMPONENT_H

class BombComponent;

#include "BombInterface.h"
#include "BombConfig.h"
#include "Common.h"
#include "GameEvent.h"

#define BOMB_VARIABLES_ARRAY(...) (const InfoStreamBuilderBase::VariableParam[]) {__VA_ARGS__, {nullptr, VAR_NULLTYPE}}
#define BOMB_NO_VARIABLES (const InfoStreamBuilderBase::VariableParam[]){{nullptr, VAR_NULLTYPE}}

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
    void Init(BombInterface* bomb);

    virtual void Configure(void* config);
    virtual void Configure(BombConfig* config);

    void ClearConfiguredFlags();

    virtual void Reset();

    virtual const InfoStreamBuilderBase::VariableParam* GetVariableInfo();

    virtual void GetInfo(void** pData, size_t* pSize);

    void BuildVariableInfo(InfoStreamBuilderBase* builder);

    virtual void Bootstrap();
    
    virtual void Standby();

    virtual void Arm();

    virtual void Update();

    virtual void Display();

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
    game::EventManager<M> m_Events;

    EventfulComponentTrait() : m_Events(static_cast<M*>(this)) {

    }
};

class BombModule : public BombComponent, NamedComponentTrait {
    
    virtual void ConfigureModule(ModuleConfig* config);
    void Configure(void* config) override; 

    virtual BombConfig::ModuleFlag GetModuleFlags();

    void GetInfo(void** pData, size_t* pSize) override;
};

class DefusableModule : public BombModule {
    
    BombConfig::ModuleFlag GetModuleFlags() override;
};

class BombPort : public BombComponent, NamedComponentTrait {

    virtual void ConfigurePort(PortConfig* config);
    void Configure(void* config) override;

    void GetInfo(void** pData, size_t* pSize) override;
};

class BombLabel : public BombComponent {

    virtual const char** GetTextOptions() = 0;

    virtual void ConfigureLabel(LabelConfig* config);
    void Configure(void* config) override;

    void GetInfo(void** pData, size_t* pSize) override;
};

class BombBattery : public BombComponent {

    virtual uint8_t GetBatteryCount() = 0;
    virtual uint8_t GetBatterySize() = 0;

    virtual void ConfigureBattery(BatteryConfig* config);
    void Configure(void* config) override;

    void GetInfo(void** pData, size_t* pSize) override;
};

#endif