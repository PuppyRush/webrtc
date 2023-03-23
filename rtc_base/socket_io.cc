/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/socket_io.h"



#include "absl/memory/memory.h"
#include "rtc_base/checks.h"

namespace rtc {

SocketIOAdapter::SocketIOAdapter(Socket* socket)
    : socket_(absl::WrapUnique(socket)) {
  RTC_DCHECK(socket_);
  socket_->SignalConnectEvent.connect(this,
                                      &SocketIOAdapter::OnConnectEvent);
  socket_->SignalReadEvent.connect(this, &SocketIOAdapter::OnReadEvent);
  socket_->SignalWriteEvent.connect(this, &SocketIOAdapter::OnWriteEvent);
  socket_->SignalCloseEvent.connect(this, &SocketIOAdapter::OnCloseEvent);

  sio_socket_ = client_.socket();
}

SocketAddress SocketIOAdapter::GetLocalAddress() const {
  return socket_->GetLocalAddress();
}

SocketAddress SocketIOAdapter::GetRemoteAddress() const {
  return socket_->GetRemoteAddress();
}

int SocketIOAdapter::Bind(const SocketAddress& addr) {

  return 0;

  return socket_->Bind(addr);
}

int SocketIOAdapter::Connect(const SocketAddress& addr) {
  
  client_.connect("https://10.10.45.51:9559");

  return 0;

  return socket_->Connect(addr);
}

void SocketIOAdapter::Emit(
    std::string const& name,
          sio::message::list const& msglist,
          std::function<void(sio::message::list const&)> const& ack) {
  sio_socket_->emit(name, msglist, ack);
}

int SocketIOAdapter::Send(const void* pv, size_t cb) {

  return socket_->Send(pv, cb);
}

int SocketIOAdapter::SendTo(const void* pv,
                               size_t cb,
                               const SocketAddress& addr) {
  return socket_->SendTo(pv, cb, addr);
}

int SocketIOAdapter::Recv(void* pv, size_t cb, int64_t* timestamp) {
  return socket_->Recv(pv, cb, timestamp);
}

int SocketIOAdapter::RecvFrom(void* pv,
                                 size_t cb,
                                 SocketAddress* paddr,
                                 int64_t* timestamp) {
  return socket_->RecvFrom(pv, cb, paddr, timestamp);
}

int SocketIOAdapter::Listen(int backlog) {
  return socket_->Listen(backlog);
}

Socket* SocketIOAdapter::Accept(SocketAddress* paddr) {
  return socket_->Accept(paddr);
}

int SocketIOAdapter::Close() {
  return socket_->Close();
}

int SocketIOAdapter::GetError() const {
  return socket_->GetError();
}

void SocketIOAdapter::SetError(int error) {
  return socket_->SetError(error);
}

Socket::ConnState SocketIOAdapter::GetState() const {
  return socket_->GetState();
}

int SocketIOAdapter::GetOption(Option opt, int* value) {
  return socket_->GetOption(opt, value);
}

int SocketIOAdapter::SetOption(Option opt, int value) {
  return socket_->SetOption(opt, value);
}

void SocketIOAdapter::OnConnectEvent(Socket* socket) {
  SignalConnectEvent(this);
}

void SocketIOAdapter::OnReadEvent(Socket* socket) {
  SignalReadEvent(this);
}

void SocketIOAdapter::OnWriteEvent(Socket* socket) {
  SignalWriteEvent(this);
}

void SocketIOAdapter::OnCloseEvent(Socket* socket, int err) {
  SignalCloseEvent(this, err);
}

}  // namespace rtc
