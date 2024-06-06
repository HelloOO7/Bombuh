#ifndef __COMPONENTMAIN_H
#define __COMPONENTMAIN_H

#include "BombComponent.h"
#include "lambda.h"
#include "UARTPrint.h"

#ifdef DEBUG
#define _STRINGIZE(x) _STRINGIZE2(x)
#define _STRINGIZE2(x) #x
#define BOMB_ASSERT(expression) if (!(expression)) { ComponentMain::GlobalAssertFailed(PSTR("[!] " __FILE__ ":" _STRINGIZE(__LINE__) ": " #expression)); }
#else
#define BOMB_ASSERT(expression)
#endif

class ComponentMain {
private:
    enum class StateRequest {
        NONE,
        STANDBY,
        ARM,
        RESET
    };

    BombClient m_BombCl;
    BombInterface* m_BombInterface;
    BombComponent* m_Component;

    bool m_IsArmed;
    StateRequest m_RequestedState;

public:
    ComponentMain();

    static ComponentMain* GetInstance();

    void Setup(BombComponent* module, bool disableSerial = false);

    void Loop();

    void DispatchEvent(uint8_t id, void* data);

    static void DoDispatchEvent(uint8_t id, void* data, ComponentMain* mm);

    void AssertFailed(const char* message);

    static void GlobalAssertFailed(const char* message);

private:
    void AssertFailedPanicLoop();
};

#endif