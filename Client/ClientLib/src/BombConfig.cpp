#include "BombConfig.h"

static void RelocatePointer(void* pptr, void* base) {
    void** _pptr = static_cast<void**>(pptr);
    void* ptr = *_pptr;
    *_pptr = static_cast<void*>(static_cast<char*>(base) + reinterpret_cast<uintptr_t>(ptr));
}

ModuleConfig* ModuleConfig::FromBuffer(void* buffer) {
    ModuleConfig* cfg = reinterpret_cast<ModuleConfig*>(buffer);
    RelocatePointer(cfg->Variables.ArrayPointer(), cfg);
    for (size_t i = 0; i < cfg->Variables.Size(); i++) {
        auto varType = cfg->Variables[i].Type;
        if (varType == VAR_STR || varType == VAR_STR_ENUM) {
            RelocatePointer(&cfg->Variables[i].StringValue, cfg);
        }
    }
    return cfg;
}

BombConfig* BombConfig::FromBuffer(void* buffer) {
    BombConfig* cfg = reinterpret_cast<BombConfig*>(buffer);
    RelocatePointer(cfg->Modules.ArrayPointer(), cfg);
    RelocatePointer(cfg->Batteries.ArrayPointer(), cfg);
    RelocatePointer(cfg->Labels.ArrayPointer(), cfg);
    RelocatePointer(cfg->Ports.ArrayPointer(), cfg);
    for (size_t i = 0; i < cfg->Modules.Size(); i++) {
        RelocatePointer(&cfg->Modules[i].ExtraData, cfg);
    }
    return cfg;
}

int BombConfig::BatteryCount() {
    int cnt = 0;
    for (size_t i = 0; i < Batteries.Size(); i++) {
        cnt += Batteries[i].Count;
    }
    return cnt;
}

bool BombConfig::IsLabelPresent(const char* name, bool mustBeLit) {
    IDHASH hash = HashID(name);
    for (size_t i = 0; i < Labels.Size(); i++) {
        if (Labels[i].Name == hash && (!mustBeLit || Labels[i].IsLit)) {
            return true;
        }
    }
    return false;
}

BombConfig::Module* BombConfig::GetModuleInfo(const char* name)  {
    IDHASH hash = HashID(name);
    for (size_t i = 0; i < Modules.Size(); i++) {
        if (Modules[i].Name == hash) {
            return &Modules[i];
        }
    }
    return nullptr;
}

ConfigVariable* ModuleConfig::GetVar(const char* name) {
    IDHASH hash = HashID(name);
    for (size_t i = 0; i < Variables.Size(); i++) {
        if (Variables[i].Name == hash) {
            return &Variables[i];
        }
    }
    DEBUG_PRINTF_P("Variable of name %s hash %08X was not found!\n", name, hash);
    return nullptr;
}

ConfigVariable* ModuleConfig::GetTypedVar(const char* name, ConfigVariableType type) {
    ConfigVariable* var = GetVar(name);
    if (var && var->Type == type) {
        return var;
    }
    return nullptr;
}

int ModuleConfig::GetEnum(const char* name, const char* const* enumValueNames, int enumMax) {
    if (auto var = GetTypedVar(name, VAR_STR_ENUM)) {
        auto value = var->StringValue;
        for (int i = 0; i < enumMax; i++) {
            if (strcasecmp(value, enumValueNames[i]) == 0) {
                return i;
            }
        }
    }
    return 0;
}

const char* ModuleConfig::GetString(const char* name) {
    if (auto var = GetTypedVar(name, VAR_STR)) {
        return var->StringValue;
    }
    return "";
}

const char* ModuleConfig:: GetPermanentString(const char* name) {
    const char* str = GetString(name);
    size_t size = strlen(str) + 1;
    char* out = new char[size];
    memcpy(out, str, size);
    return out;
}

int ModuleConfig::GetInt(const char* name) {
    if (auto var = GetTypedVar(name, VAR_INT)) {
        return var->IntValue;
    }
    return 0;
}

long ModuleConfig::GetLong(const char* name) {
    if (auto var = GetTypedVar(name, VAR_LONG)) {
        return var->IntValue;
    }
    return 0L;
}

bool ModuleConfig::GetBool(const char* name) {
    if (auto var = GetTypedVar(name, VAR_BOOL)) {
        return var->BoolValue;
    }
    return 0L;
}

InfoStreamBuilderBase::InfoStreamBuilderBase()  {
    m_BufferSize = 0;
    m_MaxOutSize = 0;
    ResizeBuffer(32);
}

InfoStreamBuilderBase::~InfoStreamBuilderBase() {
    if (m_Buffer) {
        free(m_Buffer);
    }
}

void InfoStreamBuilderBase::ResizeBuffer(size_t newSize) {
    DEBUG_PRINTF_P("Resizing stream buffer from %d to %d\n", m_BufferSize, newSize)
    if (!m_BufferSize) {
        m_Buffer = (char*)malloc(newSize);
    }
    else {
        m_Buffer = (char*)realloc(m_Buffer, newSize);
    }
    m_BufferSize = newSize;
}

size_t InfoStreamBuilderBase::WriteData(size_t address, const void* data, size_t size) {
    if (!size) {
        return address;
    }
    DEBUG_PRINTF_P("Writing %d bytes at %d\n", size, address);
    size_t endaddr = address + size;
    if (endaddr > m_BufferSize) {
        ResizeBuffer(endaddr + 16);  
    }
    memcpy(m_Buffer + address, data, size);
    if (endaddr > m_MaxOutSize) {
        m_MaxOutSize = endaddr;
    }
    return endaddr;
}

size_t InfoStreamBuilderBase::WriteString(size_t address, const char* str) {
    uint8_t len = strlen(str);
    DEBUG_PRINTF_P("Writing string %s at address %d\n", str, address);
    address = WriteData(address, len);
    address = WriteData(address, str, len);
    return address;
}

size_t InfoStreamBuilderBase::OpenHeader(BombConfig::ComponentType type)  {
    size_t addr = 0;
    addr = WriteData(addr, type);
    return addr;
}

void InfoStreamBuilderBase::FinalizeHeader(size_t addr) {
    m_VarCountAddress = addr;
    uint8_t zero = 0;
    addr = WriteData(addr, zero);
    m_VarsPointer = addr;
}

void InfoStreamBuilderBase::Build(void** data, size_t* dataSize) {
    void* d = realloc(m_Buffer, m_MaxOutSize);
    m_Buffer = nullptr;
    *data = d;
    *dataSize = m_MaxOutSize;
}

void InfoStreamBuilderBase::AddVariable(const VariableParam* param) {
    *(m_Buffer + m_VarCountAddress) += 1;
    m_VarsPointer = WriteString(m_VarsPointer, param->Name);
    m_VarsPointer = WriteData(m_VarsPointer, param->Type);
}

void BatteryInfoStreamBuilder::SetBatteryInfo(uint8_t batteryCount, uint8_t batterySize) {
    size_t addr = OpenHeader(BombConfig::ComponentType::BATTERY);
    addr = WriteData(addr, batteryCount);
    addr = WriteData(addr, batterySize);
    FinalizeHeader(addr);
}

void LabelInfoStreamBuilder::SetLabelInfo(const char** possibleTexts) {
    size_t addr = OpenHeader(BombConfig::ComponentType::LABEL);
    const char** texts = possibleTexts;
    while (*texts != nullptr) {
        texts++;
    }
    uint8_t textCount = texts - possibleTexts;
    addr = WriteData(addr, textCount);
    texts = possibleTexts;
    while (*texts != nullptr) {
        addr = WriteString(addr, *texts);
        texts++;
    }
    FinalizeHeader(addr);
}

void PortInfoStreamBuilder::SetPortInfo(const char* portName) {
    size_t addr = OpenHeader(BombConfig::ComponentType::PORT);
    addr = WriteString(addr, portName);
    FinalizeHeader(addr);
}

ModuleInfoStreamBuilder* ModuleInfoStreamBuilder::SetInfo(const char* moduleName, BombConfig::ModuleFlag flags, void* extraData, uint16_t extraDataSize) {
    size_t addr = OpenHeader(BombConfig::ComponentType::MODULE);
    addr = WriteString(addr, moduleName);
    addr = WriteData(addr, flags);
    addr = WriteData(addr, extraDataSize);
    addr = WriteData(addr, extraData, extraDataSize);
    FinalizeHeader(addr);
    return this;
}