#include "BombInterface.h"
#include "BombConfig.h"
#include "Common.h"
#include "BombComponent.h"

BombComponent::BombComponent() {

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

void BombComponent::Configure(BombConfig* config) {

}

void BombComponent::Configure(void* config) {

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

void BombComponent::GetInfo(void** pData, size_t* pSize) {
    *pSize = 0;
}

#include "MemoryFree.h"

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

void BombModule::Configure(void* config) {
    ConfigureModule(ModuleConfig::FromBuffer(config));
}

BombConfig::ModuleFlag DefusableModule::GetModuleFlags() {
    return BombConfig::ModuleFlag::DEFUSABLE;
}

void BombPort::Configure(void* config) {
    ConfigurePort(static_cast<PortConfig*>(config));
}

void BombPort::GetInfo(void** pData, size_t* pSize) {
    PortInfoStreamBuilder bld;
    bld.SetPortInfo(GetName());
    BuildVariableInfo(&bld);
    bld.Build(pData, pSize);
}

void BombLabel::Configure(void* config) {
    ConfigureLabel(static_cast<LabelConfig*>(config));
}

void BombLabel::GetInfo(void** pData, size_t* pSize) {
    LabelInfoStreamBuilder bld;
    bld.SetLabelInfo(GetTextOptions());
    BuildVariableInfo(&bld);
    bld.Build(pData, pSize);
}

void BombBattery::Configure(void* config) {
    ConfigureBattery(static_cast<BatteryConfig*>(config));
}

void BombBattery::GetInfo(void** pData, size_t* pSize) {
    BatteryInfoStreamBuilder bld;
    bld.SetBatteryInfo(GetBatteryCount(), GetBatterySize());
    BuildVariableInfo(&bld);
    bld.Build(pData, pSize);
}