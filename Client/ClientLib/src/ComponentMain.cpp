#include "BombComponent.h"
#include "lambda.h"
#include "UARTPrint.h"
#include "ComponentMain.h"

ComponentMain::ComponentMain() : m_IsArmed{false}, m_RequestedState{StateRequest::NONE} {

}

ComponentMain* ComponentMain::GetInstance() {
    static ComponentMain _inst;

    return &_inst;
}

void ComponentMain::Setup(BombComponent* module, bool disableSerial) {
    if (!disableSerial) {
        print_init(115200);
    }
    m_BombInterface = new BombInterface(&m_BombCl, module->GetSyncFlags());

    m_Component = module;
    m_Component->Init(m_BombInterface);
    m_Component->Bootstrap();
    m_Component->Standby();

    m_BombCl.SetHandshakeHandler(function(void** pData, size_t* pSize, BombComponent* module) {
        void* mainData;
        size_t mainSize;
        module->GetInfo(&mainData, &mainSize);
        size_t allSize = mainSize + sizeof(ServerCommConfig);
        void* allData = realloc(mainData, allSize);
        memmove(reinterpret_cast<char*>(allData) + sizeof(ServerCommConfig), allData, mainSize);
        ServerCommConfig* commCfg = reinterpret_cast<ServerCommConfig*>(allData);
        commCfg->AcceptsEvents = module->GetAcceptedEvents() | bconf::ALWAYS_LISTEN_BITS;
        *pData = allData;
        *pSize = allSize;
    }, m_Component);
    m_BombCl.AddEventDispatcher(DoDispatchEvent, this);

    int addr = AddressObtainer::FromAnalogPin(A6);
    PRINTF_P("Address: %d\n", addr);
    m_BombCl.Attach(addr);
    PRINTLN_P("Setup done.");
}

void ComponentMain::Loop() {
    if (m_RequestedState != StateRequest::NONE && m_BombCl.IsAllSyncDone()) {
        PROCESS_STATE_CHANGE:
        switch (m_RequestedState) {
            case StateRequest::ARM:
                m_IsArmed = true;
                m_Component->Arm();
                break;
            case StateRequest::STANDBY:
                m_IsArmed = false;
                m_Component->Standby();
                break;
            case StateRequest::RESET:
                m_Component->Reset();
                m_RequestedState = StateRequest::STANDBY;
                goto PROCESS_STATE_CHANGE;
            default:
                break;
        }
        m_RequestedState = StateRequest::NONE;
    }
    if (m_IsArmed) {
        if (m_BombCl.IsAllSyncDone()) {
            m_Component->Update();
        }
        m_Component->Display();
    }
    else {
        m_Component->IdleDisplay();
    }
}

void ComponentMain::DispatchEvent(uint8_t id, void* data) {
    DEBUG_PRINTF_P("Component BombEvent received %d\n", id)
    switch (id) {
        case bconf::BombEvent::CONFIGURE:
            DEBUG_PRINTLN_P("Begin configuration")
            m_Component->ClearConfiguredFlags();
            if (m_Component->GetSyncFlags() & bconf::FETCH_CONFIG) {
                m_BombInterface->LoadBombConfig(m_Component);
            }
            m_BombInterface->LoadComponentConfig(m_Component);
            break;
        case bconf::BombEvent::RESET:
        case bconf::BombEvent::EXPLOSION:
            m_RequestedState = StateRequest::RESET;
            break;
        case bconf::BombEvent::DEFUSAL:
            m_RequestedState = StateRequest::STANDBY;
            break;
        case bconf::BombEvent::ARM:
            m_RequestedState = StateRequest::ARM;
            break;
    }
    m_Component->OnEvent(id, data);
}

void ComponentMain::DoDispatchEvent(uint8_t id, void* data, ComponentMain* mm) {
    mm->DispatchEvent(id, data);
}