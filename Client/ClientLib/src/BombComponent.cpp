#include "BombInterface.h"
#include "BombConfig.h"
#include "Common.h"
#include "BombComponent.h"

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
    m_BombConfigDone = false;
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
    m_LightOffTime = millis() + 1000;
    GetModuleLedDriver()->TurnOn(0xFF0000);
    m_Bomb->Strike();
}

void DefusableModule::Bootstrap() {
    GetModuleLedDriver()->Init();
}

void DefusableModule::Reset() {
    GetModuleLedDriver()->TurnOff();
}

void DefusableModule::Standby() {
}

void DefusableModule::Arm() {
    m_IsDefused = false;
}

void DefusableModule::Update() {
    if (m_LightOffTime) {
        if (millis() >= m_LightOffTime) {
            m_LightOffTime = 0;
            GetModuleLedDriver()->TurnOff();
        }
    }
    if (!m_IsDefused) {
        ActiveUpdate();
    }
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