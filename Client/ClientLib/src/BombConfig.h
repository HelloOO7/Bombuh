#ifndef __BOMBCONFIG_H
#define __BOMBCONFIG_H

#include "Common.h"
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include "EnumFlagOperators.h"
#include <string.h>
#include "DebugPrint.h"

#define LABEL_TEXTS_ARRAY(...) {__VA_ARGS__, nullptr}

enum ConfigVariableType : uint8_t {
    VAR_NULLTYPE,
    VAR_STR,
    VAR_INT,
    VAR_LONG,
    VAR_BOOL
};

struct ConfigVariable {
    IDHASH      Name;
    ConfigVariableType Type;
    union {
        const char* StringValue;
        uint16_t    IntValue;
        uint32_t    LongValue;
        bool        BoolValue;
    };
};

AVR_SASSERT(sizeof(ConfigVariable) == 9)

struct LabelConfig {
    IDHASH  Text;
    uint8_t IsLit;
};

struct PortConfig
{
    uint8_t IsUsed;
};

struct BatteryConfig
{
    uint8_t IsUsed;
};

struct ModuleConfig {
    FixedArrayRef<ConfigVariable> Variables;

    ConfigVariable* GetVar(const char* name);
    ConfigVariable* GetTypedVar(const char* name, ConfigVariableType type);
    
    const char* GetString(const char* name);
    const char* GetPermanentString(const char* name);

    int GetInt(const char* name);

    long GetLong(const char* name);

    bool GetBool(const char* name);

    static ModuleConfig* FromBuffer(void* buffer);
};

struct BombConfig
{
    static constexpr int SERIAL_NUMBER_LENGTH = 16;

    enum ComponentType : uint8_t {
        MODULE,
        LABEL,
        PORT,
        BATTERY
    };

    enum ModuleFlag : uint8_t {
        NONE = 0,
        NEEDY = (1 << 0),
        DECORATIVE = (1 << 1),
        DEFUSABLE = (1 << 2)
    };

    enum SerialFlag : uint16_t {
        CONTAINS_VOWEL = (1 << 0),
        LAST_DIGIT_EVEN = (1 << 1),
        LAST_DIGIT_ODD = (1 << 2)
    };

    struct Label {
        IDHASH Name;
        bool   IsLit;
    };

    AVR_SASSERT(sizeof(Label) == 5);

    struct Port {
        IDHASH Name;
    };

    struct Battery {
        uint8_t Count;
        uint8_t Size;
    };

    struct Module {
        IDHASH Name;
        ModuleFlag Flags;
        void*  ExtraData;
    };

    AVR_SASSERT(sizeof(Module) == 7);
    
    uint32_t    RandomSeed;

    char        SerialNo[SERIAL_NUMBER_LENGTH];
    SerialFlag  SerialFlags;
    
    uint8_t     MaxStrikes;
    bombclock_t TimeLimit;

    FixedArrayRef<Module>  Modules;
    FixedArrayRef<Label>   Labels;
    FixedArrayRef<Port>    Ports;
    FixedArrayRef<Battery> Batteries;

    Module* GetModuleInfo(const char* name);

    static BombConfig* FromBuffer(void* buffer);
};

DEFINE_ENUM_FLAG_OPERATORS(BombConfig::ModuleFlag);
DEFINE_ENUM_FLAG_OPERATORS(BombConfig::SerialFlag);

class InfoStreamBuilderBase {
    //This builds a module info stream for the Python server
    //The output is NOT suitable for being used on the client!
public:
    struct VariableParam {
        const char* Name;
        ConfigVariableType Type;
    };
private:
    char*   m_Buffer;
    size_t  m_BufferSize;
    size_t  m_MaxOutSize;
    size_t  m_VarsPointer;
    size_t  m_VarCountAddress;

public:
    InfoStreamBuilderBase() {
        m_BufferSize = 0;
        m_MaxOutSize = 0;
        ResizeBuffer(32);
    }

    ~InfoStreamBuilderBase() {
        if (m_Buffer) {
            free(m_Buffer);
        }
    }

protected:
    void ResizeBuffer(size_t newSize) {
        DEBUG_PRINTF_P("Resizing stream buffer from %d to %d\n", m_BufferSize, newSize)
        if (!m_BufferSize) {
            m_Buffer = (char*)malloc(newSize);
        }
        else {
            m_Buffer = (char*)realloc(m_Buffer, newSize);
        }
        m_BufferSize = newSize;
    }

    size_t WriteData(size_t address, const void* data, size_t size) {
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

    template<typename D>
    size_t WriteData(size_t address, D* data) {
        return WriteData(address, data, sizeof(D));
    }

    template<typename D>
    size_t WriteData(size_t address, D data) {
        return WriteData(address, &data, sizeof(D));
    }

    size_t WriteString(size_t address, const char* str) {
        uint8_t len = strlen(str);
        DEBUG_PRINTF_P("Writing string %s at address %d\n", str, address);
        address = WriteData(address, len);
        address = WriteData(address, str, len);
        return address;
    }

    size_t OpenHeader(BombConfig::ComponentType type) {
        size_t addr = 0;
        addr = WriteData(addr, type);
        return addr;
    }

    void FinalizeHeader(size_t addr) {
        m_VarCountAddress = addr;
        uint8_t zero = 0;
        addr = WriteData(addr, zero);
        m_VarsPointer = addr;
    }

public:
    void Build(void** data, size_t* dataSize) {
        void* d = realloc(m_Buffer, m_MaxOutSize);
        m_Buffer = nullptr;
        *data = d;
        *dataSize = m_MaxOutSize;
    }

    void AddVariable(const VariableParam* param) {
        *(m_Buffer + m_VarCountAddress) += 1;
        m_VarsPointer = WriteString(m_VarsPointer, param->Name);
        m_VarsPointer = WriteData(m_VarsPointer, param->Type);
    }
};

class BatteryInfoStreamBuilder : public InfoStreamBuilderBase {
public:
    void SetBatteryInfo(uint8_t batteryCount, uint8_t batterySize) {
        size_t addr = OpenHeader(BombConfig::ComponentType::BATTERY);
        addr = WriteData(addr, batteryCount);
        addr = WriteData(addr, batterySize);
        FinalizeHeader(addr);
    }
};

class LabelInfoStreamBuilder : public InfoStreamBuilderBase {
public:
    void SetLabelInfo(const char** possibleTexts) {
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
};

class PortInfoStreamBuilder : public InfoStreamBuilderBase {
public:
    void SetPortInfo(const char* portName) {
        size_t addr = OpenHeader(BombConfig::ComponentType::PORT);
        addr = WriteString(addr, portName);
        FinalizeHeader(addr);
    }
};

class ModuleInfoStreamBuilder : public InfoStreamBuilderBase {
public:
    ModuleInfoStreamBuilder* SetInfo(const char* moduleName, BombConfig::ModuleFlag flags, void* extraData, uint16_t extraDataSize) {
        size_t addr = OpenHeader(BombConfig::ComponentType::MODULE);
        addr = WriteString(addr, moduleName);
        addr = WriteData(addr, flags);
        addr = WriteData(addr, extraDataSize);
        addr = WriteData(addr, extraData, extraDataSize);
        FinalizeHeader(addr);
        return this;
    }

    template<typename E>
    ModuleInfoStreamBuilder* SetInfo(const char* moduleName, BombConfig::ModuleFlag flags, E* extraData = nullptr) {
        return SetInfo(moduleName, flags, extraData, sizeof(E));
    }

    inline ModuleInfoStreamBuilder* SetInfo(const char* moduleName, BombConfig::ModuleFlag flags) {
        return SetInfo(moduleName, flags, nullptr, 0);
    }
};


#endif