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
    VAR_BOOL,
    VAR_STR_ENUM
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

struct ServerCommConfig {
    uint32_t AcceptsEvents;
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

    int GetEnum(const char* name, const char* const* enumValueNames, int enumMax);

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

    int BatteryCount();
    bool IsLabelPresent(const char* name, bool mustBeLit = true);

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
    InfoStreamBuilderBase();

    ~InfoStreamBuilderBase();

protected:
    void ResizeBuffer(size_t newSize);

    size_t WriteData(size_t address, const void* data, size_t size);

    template<typename D>
    size_t WriteData(size_t address, D* data) {
        return WriteData(address, data, sizeof(D));
    }

    template<typename D>
    size_t WriteData(size_t address, D data) {
        return WriteData(address, &data, sizeof(D));
    }

    size_t WriteString(size_t address, const char* str);

    size_t OpenHeader(BombConfig::ComponentType type);
    void FinalizeHeader(size_t addr);

public:
    void Build(void** data, size_t* dataSize);

    void AddVariable(const VariableParam* param);
};

class BatteryInfoStreamBuilder : public InfoStreamBuilderBase {
public:
    void SetBatteryInfo(uint8_t batteryCount, uint8_t batterySize);
};

class LabelInfoStreamBuilder : public InfoStreamBuilderBase {
public:
    void SetLabelInfo(const char** possibleTexts);
};

class PortInfoStreamBuilder : public InfoStreamBuilderBase {
public:
    void SetPortInfo(const char* portName);
};

class ModuleInfoStreamBuilder : public InfoStreamBuilderBase {
public:
    ModuleInfoStreamBuilder* SetInfo(const char* moduleName, BombConfig::ModuleFlag flags, void* extraData, uint16_t extraDataSize);

    template<typename E>
    ModuleInfoStreamBuilder* SetInfo(const char* moduleName, BombConfig::ModuleFlag flags, E* extraData = nullptr) {
        return SetInfo(moduleName, flags, extraData, sizeof(E));
    }

    inline ModuleInfoStreamBuilder* SetInfo(const char* moduleName, BombConfig::ModuleFlag flags) {
        return SetInfo(moduleName, flags, nullptr, 0);
    }
};


#endif