#include "BombClient.h"
#include "BombInterface.h"
#include "Arduino.h"
#include "AddressObtainer.h"
#include "AsyncI2CLib.h"
#include "UARTPrint.h"

BombClient cl;

struct TestResponse : BombClient::TResponse {
    char Message[16];
};

struct TestRequest : BombClient::TRequest<TestResponse> {
    char Message[16];
};

void HandleTestResponse(TestResponse* resp, BombClient* cl) {
    PRINTF_P("Response: %s\n", resp->Message);
    TestRequest r;
    memcpy(&r.Message, "ahoj", strlen("ahoj") + 1);
    cl->QueueRequest<TestResponse>("Test", &r, HandleTestResponse, cl);
}

void setup() {
    print_init(115200);
    int addr = AddressObtainer::FromAnalogPin(A6);
    PRINTF_P("Address: %d\n", addr);
    cl.Attach(addr);
    TestRequest r;
    memcpy(&r.Message, "ahoj", strlen("ahoj") + 1);
    cl.QueueRequest<TestResponse>("Test", &r, HandleTestResponse, &cl);
    puts("Client started.");
}

void loop() {
    delay(100);   
}