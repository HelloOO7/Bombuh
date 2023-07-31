// GameEventTest.cpp : Tento soubor obsahuje funkci main. Provádění programu se tam zahajuje a ukončuje.
//

#include <iostream>
#include "GameEvent.h"

#include <thread>

game::EventManager<int> mgr(new int{ 1 });

using Event = game::Event<int>;

bool func(Event* e, int* c) {
	e->Then(func);
	return true;
}

int main()
{
	mgr.Start(new Event([](Event* e, int* c) {
		return true;
	}))->Then([](Event* e, int* c) {
		e->Then(func);
		return true;
	});

	for (int i= 0 ; i < 1; i++) {
		mgr.Update();
	}
	mgr.CancelAll();
	mgr.Update();
}