#include "BombInterface.h"
#include "BombConfig.h"
#include "Common.h"
#include "BombComponent.h"

const InfoStreamBuilderBase::VariableParam __BOMB_NO_VARIABLES[]{
    {nullptr, VAR_NULLTYPE}
};

BombComponent::BombComponent() {

}

BombComponent::~BombComponent() {

}

void BombComponent::Init(BombInterface* bomb) {
    m_Bomb = bomb;
    const InfoStreamBuilderBase::VariableParam* vars = GetVariableInfo();
    m_VariableCount = 0;
    while (vars->Type != VAR_NULLTYPE) {
        m_VariableCount++;
        vars++;
    }
}

void BombComponent::LoadConfiguration(BombConfig* config) {

}

void BombComponent::LoadConfiguration(void* config) {

}

void BombComponent::Configure() {
    
}

void BombComponent::ClearConfiguredFlags() {
    m_BombConfigDone = !(GetSyncFlags() & bconf::FETCH_CONFIG);
    m_ModuleConfigDone = false;
}

void BombComponent::Reset() {

}

const InfoStreamBuilderBase::VariableParam* BombComponent::GetVariableInfo() {
    return BOMB_NO_VARIABLES;
}

bconf::BombEventBit BombComponent::GetAcceptedEvents() {
    return bconf::NONE_BITS;
}

void BombComponent::GetInfo(void** pData, size_t* pSize) {
    *pSize = 0;
}

void BombComponent::BuildVariableInfo(InfoStreamBuilderBase* builder) {
    const InfoStreamBuilderBase::VariableParam* vars = GetVariableInfo();
    while (vars->Type != VAR_NULLTYPE) {
        DEBUG_PRINTF_P("Adding variable %s\n", *vars);
        builder->AddVariable(vars++);
    }
}

void BombComponent::Bootstrap() {

}

void BombComponent::Standby() {

}

void BombComponent::Arm() {

}

void BombComponent::Update() {

}

void BombComponent::Display() {

}

void BombComponent::IdleDisplay() {

}

void BombComponent::OnEvent(uint8_t id, void* data) {

}

bconf::SyncFlag BombComponent::GetSyncFlags() {
    return bconf::SyncFlag::SYNC_NOTHING;
}

const char* NamedComponentTrait::GetName() {
    return "";
}

void BombModule::GetInfo(void** pData, size_t* pSize) {
    ModuleInfoStreamBuilder bld;
    bld.SetInfo(GetName(), GetModuleFlags());
    BuildVariableInfo(&bld);
    bld.Build(pData, pSize);
}

void BombModule::LoadConfiguration(void* config) {
    LoadConfiguration(ModuleConfig::FromBuffer(config));
}

BombConfig::ModuleFlag DefusableModule::GetModuleFlags() {
    return BombConfig::ModuleFlag::DEFUSABLE;
}

void DefusableModule::Defuse() {
    m_IsDefused = true;
    m_Bomb->DefuseMe();
    GetModuleLedDriver()->TurnOn(0x00FF00);
    PRINTF_P("Module %s defused.\n", GetName());
}

void DefusableModule::Strike() {
    GetModuleLedDriver()->TurnOn(0xFF0000);
    game::Event<ModuleLedDriver>* turnOff = game::CreateWaitEvent<ModuleLedDriver>(1000);
    turnOff->Then(new game::Event<ModuleLedDriver>(function(game::Event<ModuleLedDriver>* e, ModuleLedDriver* drv) {
        drv->TurnOff();
        return true;
    }));
    m_LightEvents->Start(turnOff);
    m_Bomb->Strike();
}

static bool FlashConfigLedEvent(game::Event<ModuleLedDriver>* e, ModuleLedDriver* drv, bool* data) {
    bool on = *data;
    if (on) {
        drv->TurnOn(0x0000FF);
    }
    else {
        drv->TurnOff();
    }
    e
    ->Then(game::CreateWaitEvent<ModuleLedDriver>(500))
    ->Then(new game::Event<ModuleLedDriver>(FlashConfigLedEvent, new bool{!on}));
    return true;
}

struct EventModuleLedScheduleParam {
    bool m_On;
    DefusableModule* m_Module;
};

void DefusableModule::OnEvent(uint8_t id, void* data) {
    if (id == bconf::CONFIG_LIGHT) {
        //Inside ISR - ensure atomicity
        auto param = new EventModuleLedScheduleParam{
            *static_cast<uint8_t*>(data) == 1,
            this  
        };
        game::Event<ModuleLedDriver>* sched = new game::Event<ModuleLedDriver>(function(game::Event<ModuleLedDriver>* e, ModuleLedDriver* drv, EventModuleLedScheduleParam* data) {
            if (data->m_On) {
                e->Then(FlashConfigLedEvent, new bool{true});
                drv->TurnOn(0x0000FF);
            }
            else {
                data->m_Module->TurnOffLed();
            }
            return true;
        }, param);
        if (!m_LightEventQueue->QueueExclusive(sched, m_LightMutex)) {
            delete sched;
        }
    }
}

void DefusableModule::Bootstrap() {
    GetModuleLedDriver()->Init();
    m_LightEvents = new game::EventManager<ModuleLedDriver>(GetModuleLedDriver());
    m_LightEventQueue = new game::EventQueue<ModuleLedDriver>(m_LightEvents);
}

void DefusableModule::TurnOffLed() {
    GetModuleLedDriver()->TurnOff();
    m_LightEvents->CancelAll();
    m_LightEventQueue->Clear();
}

void DefusableModule::Reset() {
    TurnOffLed();
}

void DefusableModule::Standby() {
}

void DefusableModule::Arm() {
    m_IsDefused = false;
    TurnOffLed();
}

void DefusableModule::Update() {
    m_LightEventQueue->Execute();
    m_LightEvents->Update();
    if (!m_IsDefused) {
        ActiveUpdate();
    }
}

void DefusableModule::IdleDisplay() {
    m_LightEventQueue->Execute();
    m_LightEvents->Update();
}

void DefusableModule::ActiveUpdate() {

}

void BombPort::LoadConfiguration(void* config) {
    LoadConfiguration(static_cast<PortConfig*>(config));
}

void BombPort::GetInfo(void** pData, size_t* pSize) {
    PortInfoStreamBuilder bld;
    bld.SetPortInfo(GetName());
    BuildVariableInfo(&bld);
    bld.Build(pData, pSize);
}

void BombLabel::LoadConfiguration(void* config) {
    LoadConfiguration(static_cast<LabelConfig*>(config));
}

void BombLabel::GetInfo(void** pData, size_t* pSize) {
    LabelInfoStreamBuilder bld;
    bld.SetLabelInfo(GetTextOptions());
    BuildVariableInfo(&bld);
    bld.Build(pData, pSize);
}

void BombBattery::LoadConfiguration(void* config) {
    LoadConfiguration(static_cast<BatteryConfig*>(config));
}

void BombBattery::GetInfo(void** pData, size_t* pSize) {
    BatteryInfoStreamBuilder bld;
    bld.SetBatteryInfo(GetBatteryCount(), GetBatterySize());
    BuildVariableInfo(&bld);
    bld.Build(pData, pSize);
}