#pragma once
#include "station.hpp"
// Implement in the carrier-board component. poll must return immediately.
// enter starts the physical action associated with a newly journaled state.
// Network jobs are dispatched separately. stop disables all movement.
extern "C" bool maintenance_sd_ready();
extern "C" bool board_init();
extern "C" void board_stop();
extern "C" bool board_enter(conveyor::State,const Config&,const Parcel&);
extern "C" bool board_poll(conveyor::State,conveyor::Event&,Parcel&);
