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
        if (cfg->Variables[i].Type == VAR_STR) {
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

const char* ModuleConfig:: GetString(const char* name) {
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