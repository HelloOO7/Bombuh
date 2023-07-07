#ifndef __COMPONENTMAIN_H
#define __COMPONENTMAIN_H

#include "BombComponent.h"
#include "lambda.h"
#include "UARTPrint.h"

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

    void Setup(BombComponent* module);

    void Loop();

    void DispatchEvent(uint8_t id, void* data);

    static void DoDispatchEvent(uint8_t id, void* data, ComponentMain* mm);
};

#endif