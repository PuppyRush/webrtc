#pragma once

#include "sio_client.h"

using namespace sio;

void OnMessage(sio::event& e) {

	auto data = e.get_message();
}


void socketio_main() {
  sio::client h;

    h.set_open_listener([]() { printf("open"); });
  h.set_close_listener(
      [](sio::client::close_reason const& reason) { printf("close"); });
  h.set_fail_listener([]() { printf("fail"); });
  h.set_socket_open_listener(
      [](std::string const& nsp) { printf("set_socket_open_listener"); });

  h.set_logs_verbose();
  h.connect("https://10.10.45.51:9559");


  // emit event name only:
 // h.socket()->emit("connection");

 //// emit binary
 // char buf[100];
 // h.socket()->emit("message", std::make_shared<std::string>(buf, 100));


  //h.socket()->on("message", &OnMessage);

  while (1) {
  }
}

