/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKE_DATA_CHANNEL_CONTROLLER_H_
#define PC_TEST_FAKE_DATA_CHANNEL_CONTROLLER_H_

#include <set>
#include <string>

#include "pc/sctp_data_channel.h"
#include "rtc_base/checks.h"
#include "rtc_base/weak_ptr.h"

class FakeDataChannelController
    : public webrtc::SctpDataChannelControllerInterface {
 public:
  FakeDataChannelController()
      : send_blocked_(false),
        transport_available_(false),
        ready_to_send_(false),
        transport_error_(false) {}
  virtual ~FakeDataChannelController() {}

  rtc::WeakPtr<FakeDataChannelController> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  rtc::scoped_refptr<webrtc::SctpDataChannel> CreateDataChannel(
      absl::string_view label,
      webrtc::InternalDataChannelInit init,
      rtc::Thread* network_thread = rtc::Thread::Current()) {
    rtc::scoped_refptr<webrtc::SctpDataChannel> channel =
        webrtc::SctpDataChannel::Create(weak_ptr(), std::string(label),
                                        transport_available_, init,
                                        rtc::Thread::Current(), network_thread);
    connected_channels_.insert(channel.get());
    return channel;
  }

  bool SendData(int sid,
                const webrtc::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result) override {
    RTC_CHECK(ready_to_send_);
    RTC_CHECK(transport_available_);
    if (send_blocked_) {
      *result = cricket::SDR_BLOCK;
      return false;
    }

    if (transport_error_) {
      *result = cricket::SDR_ERROR;
      return false;
    }

    last_sid_ = sid;
    last_send_data_params_ = params;
    return true;
  }

  void AddSctpDataStream(int sid) override {
    RTC_CHECK(sid >= 0);
    if (!transport_available_) {
      return;
    }
    send_ssrcs_.insert(sid);
    recv_ssrcs_.insert(sid);
  }

  void RemoveSctpDataStream(int sid) override {
    RTC_CHECK(sid >= 0);
    send_ssrcs_.erase(sid);
    recv_ssrcs_.erase(sid);
    // Unlike the real SCTP transport, act like the closing procedure finished
    // instantly, doing the same snapshot thing as below.
    auto it = absl::c_find_if(connected_channels_,
                              [&](const auto* c) { return c->id() == sid; });
    // This path mimics the DCC's OnChannelClosed handler since the FDCC
    // (this class) doesn't have a transport that would do that.
    if (it != connected_channels_.end())
      (*it)->OnClosingProcedureComplete();
  }

  bool ReadyToSendData() const override { return ready_to_send_; }

  void OnChannelStateChanged(
      webrtc::SctpDataChannel* data_channel,
      webrtc::DataChannelInterface::DataState state) override {
    if (state == webrtc::DataChannelInterface::DataState::kOpen) {
      ++channels_opened_;
    } else if (state == webrtc::DataChannelInterface::DataState::kClosed) {
      ++channels_closed_;
      connected_channels_.erase(data_channel);
    }
  }

  // Set true to emulate the SCTP stream being blocked by congestion control.
  void set_send_blocked(bool blocked) {
    send_blocked_ = blocked;
    if (!blocked) {
      RTC_CHECK(transport_available_);
      // Make a copy since `connected_channels_` may change while
      // OnTransportReady is called.
      auto copy = connected_channels_;
      for (webrtc::SctpDataChannel* ch : copy) {
        ch->OnTransportReady();
      }
    }
  }

  // Set true to emulate the transport channel creation, e.g. after
  // setLocalDescription/setRemoteDescription called with data content.
  void set_transport_available(bool available) {
    transport_available_ = available;
  }

  // Set true to emulate the transport ReadyToSendData signal when the transport
  // becomes writable for the first time.
  void set_ready_to_send(bool ready) {
    RTC_CHECK(transport_available_);
    ready_to_send_ = ready;
    if (ready) {
      std::set<webrtc::SctpDataChannel*>::iterator it;
      for (it = connected_channels_.begin(); it != connected_channels_.end();
           ++it) {
        (*it)->OnTransportReady();
      }
    }
  }

  void set_transport_error() { transport_error_ = true; }

  int last_sid() const { return last_sid_; }
  const webrtc::SendDataParams& last_send_data_params() const {
    return last_send_data_params_;
  }

  bool IsConnected(webrtc::SctpDataChannel* data_channel) const {
    return connected_channels_.find(data_channel) != connected_channels_.end();
  }

  bool IsSendStreamAdded(uint32_t stream) const {
    return send_ssrcs_.find(stream) != send_ssrcs_.end();
  }

  bool IsRecvStreamAdded(uint32_t stream) const {
    return recv_ssrcs_.find(stream) != recv_ssrcs_.end();
  }

  int channels_opened() const { return channels_opened_; }
  int channels_closed() const { return channels_closed_; }

 private:
  int last_sid_;
  webrtc::SendDataParams last_send_data_params_;
  bool send_blocked_;
  bool transport_available_;
  bool ready_to_send_;
  bool transport_error_;
  int channels_closed_ = 0;
  int channels_opened_ = 0;
  std::set<webrtc::SctpDataChannel*> connected_channels_;
  std::set<uint32_t> send_ssrcs_;
  std::set<uint32_t> recv_ssrcs_;
  rtc::WeakPtrFactory<FakeDataChannelController> weak_factory_{this};
};
#endif  // PC_TEST_FAKE_DATA_CHANNEL_CONTROLLER_H_
