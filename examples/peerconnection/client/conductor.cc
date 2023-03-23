/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/peerconnection/client/conductor.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <random>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/media_stream_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "examples/peerconnection/client/defaults.h"
#include "media/base/adapted_video_track_source.h"
#include "media/engine/multiplex_codec_factory.h"
#include "media/engine/internal_decoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "p2p/base/port_allocator.h"
#include "pc/media_stream.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/strings/json.h"
#include "test/vcm_capturer.h"

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_and_cursor_composer.h"

#include "wrapper/inc/windowcapturer.h"
#include "wrapper/inc/CapturerFactory.h"

#include "common.h"

namespace {
// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static rtc::scoped_refptr<DummySetSessionDescriptionObserver> Create() {
    return rtc::make_ref_counted<DummySetSessionDescriptionObserver>();
  }
  virtual void OnSuccess() { RTC_LOG(LS_INFO) << __FUNCTION__; }
  virtual void OnFailure(webrtc::RTCError error) {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
                     << error.message();
  }
};


}  // namespace

Conductor::Conductor(PeerConnectionClient* client, MainWindow* main_wnd)
  : loopback_(false), client_(client), main_wnd_(main_wnd) {
  client_->RegisterObserver(this);
  main_wnd->RegisterObserver(this);
}

Conductor::~Conductor() {
  RTC_DCHECK(!peer_connection_);
}

bool Conductor::connection_active() const {
  return peer_connection_ != nullptr;
}

void Conductor::Close() {
  //client_->SignOut();
  DeletePeerConnection();
}

bool Conductor::InitializePeerConnection() {
  RTC_DCHECK(!peer_connection_factory_);
  RTC_DCHECK(!peer_connection_);

  if (!signaling_thread_.get()) {
    signaling_thread_ = rtc::Thread::CreateWithSocketServer();
    signaling_thread_->Start();
  }
  peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
      nullptr /* network_thread */, nullptr /* worker_thread */,
      signaling_thread_.get(), nullptr /* default_adm */,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
      nullptr /* audio_processing */);

  if (!peer_connection_factory_) {
    main_wnd_->MessageBox("Error", "Failed to initialize PeerConnectionFactory",
                          true);
    DeletePeerConnection();
    return false;
  }

  if (!CreatePeerConnection()) {
    main_wnd_->MessageBox("Error", "CreatePeerConnection failed", true);
    DeletePeerConnection();
  }

  AddTracks();

  return peer_connection_ != nullptr;
}

bool Conductor::ReinitializePeerConnectionForLoopback() {
  loopback_ = true;
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> senders =
      peer_connection_->GetSenders();
  peer_connection_ = nullptr;
  // Loopback is only possible if encryption is disabled.
  webrtc::PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  peer_connection_factory_->SetOptions(options);
  if (CreatePeerConnection()) {
    for (const auto& sender : senders) {
      peer_connection_->AddTrack(sender->track(), sender->stream_ids());
    }
    peer_connection_->CreateOffer(
        this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  }
  options.disable_encryption = false;
  peer_connection_factory_->SetOptions(options);
  return peer_connection_ != nullptr;
}

bool Conductor::CreatePeerConnection() {
  RTC_DCHECK(peer_connection_factory_);
  RTC_DCHECK(!peer_connection_);

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  webrtc::PeerConnectionInterface::IceServer server;
  server.uri = GetPeerConnectionString();
  config.servers.push_back(server);

  webrtc::PeerConnectionDependencies pc_dependencies(this);
  auto error_or_peer_connection =
      peer_connection_factory_->CreatePeerConnectionOrError(
          config, std::move(pc_dependencies));
  if (error_or_peer_connection.ok()) {
    peer_connection_ = std::move(error_or_peer_connection.value());
  }
  return peer_connection_ != nullptr;
}

void Conductor::DeletePeerConnection() {
  main_wnd_->StopLocalRenderer();
  main_wnd_->StopRemoteRenderer();
  peer_connection_ = nullptr;
  peer_connection_factory_ = nullptr;
  loopback_ = false;
}

void Conductor::EnsureStreamingUI() {
  RTC_DCHECK(peer_connection_);
  if (main_wnd_->IsWindow()) {
    if (main_wnd_->current_ui() != MainWindow::UI::STREAMING)
      main_wnd_->SwitchToStreamingUI();
  }
}

//
// PeerConnectionObserver implementation.
//

void Conductor::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
        streams) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << receiver->id();
  main_wnd_->QueueUIThreadCallback(NEW_TRACK_ADDED,
                                   receiver->track().release());
}

void Conductor::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << receiver->id();
  main_wnd_->QueueUIThreadCallback(TRACK_REMOVED, receiver->track().release());
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();
  // For loopback test. To save some connecting delay.
  if (loopback_) {
    if (!peer_connection_->AddIceCandidate(candidate)) {
      RTC_LOG(LS_WARNING) << "Failed to apply the received candidate";
    }
    return;
  }

  Json::Value jmessage;
  jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
  jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
  std::string sdp;
  if (!candidate->ToString(&sdp)) {
    RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
    return;
  }
  jmessage[kCandidateSdpName] = sdp;

  Json::StreamWriterBuilder factory;
  SendMessage(Json::writeString(factory, jmessage));
}

//
// PeerConnectionClientObserver implementation.
//

void Conductor::StartInvate() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  //main_wnd_->SwitchToPeerList(client_->peers());

  CreateInvate();
}

void Conductor::OnDisconnected() {
  RTC_LOG(LS_INFO) << __FUNCTION__;

  DeletePeerConnection();

  if (main_wnd_->IsWindow())
    main_wnd_->SwitchToConnectUI();
}

void Conductor::OnPeerConnected(std::string user_token, const std::string& name) {
  RTC_LOG(LS_INFO) << __FUNCTION__;

  CreateInvate();

  /*if (main_wnd_->current_ui() == MainWindow::LIST_PEERS)
    main_wnd_->SwitchToPeerList(client_->peers());*/
}

void Conductor::OnPeerDisconnected(std::string user_token) {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  if (user_token == client_->user_token()) {
    RTC_LOG(LS_INFO) << "Our peer disconnected";
    main_wnd_->QueueUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
  } else {
    // Refresh the list if we're showing it.
    /*if (main_wnd_->current_ui() == MainWindow::LIST_PEERS)
      main_wnd_->SwitchToPeerList(client_->peers());*/
  }
}

void Conductor::OnMessageFromPeer(std::string user_token, const std::string& message) {
  RTC_DCHECK(client_->user_token() == user_token || client_->user_token().size() > 0);
  RTC_DCHECK(!message.empty());

  client_->peers().insert(user_token);

  if (!peer_connection_.get()) {
    //user_token_ = user_token;

    if (!InitializePeerConnection()) {
      RTC_LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
      //client_->SignOut();
      return;
    }
  }
  /*else if (user_token != user_token_) {
    RTC_LOG(LS_WARNING)
        << "Received a message from unknown peer while already in a "
           "conversation with a different peer.";
    return;
  }*/

  Json::CharReaderBuilder factory;
  std::unique_ptr<Json::CharReader> reader =
      absl::WrapUnique(factory.newCharReader());
  Json::Value jmessage;
  if (!reader->parse(message.data(), message.data() + message.length(),
                     &jmessage, nullptr)) {
    RTC_LOG(LS_WARNING) << "Received unknown message. " << message;
    return;
  }
  std::string type_str;
  std::string json_object;

  rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName,
                               &type_str);
  if (!type_str.empty()) {
    if (type_str == "offer-loopback") {
      // This is a loopback call.
      // Recreate the peerconnection with DTLS disabled.
      if (!ReinitializePeerConnectionForLoopback()) {
        RTC_LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
        DeletePeerConnection();
        //client_->SignOut();
      }
      return;
    }
    absl::optional<webrtc::SdpType> type_maybe =
        webrtc::SdpTypeFromString(type_str);
    if (!type_maybe) {
      RTC_LOG(LS_ERROR) << "Unknown SDP type: " << type_str;
      return;
    }
    webrtc::SdpType type = *type_maybe;
    std::string sdp;
    if (!rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName,
                                      &sdp)) {
      RTC_LOG(LS_WARNING)
          << "Can't parse received session description message.";
      return;
    }
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(type, sdp, &error);
    if (!session_description) {
      RTC_LOG(LS_WARNING)
          << "Can't parse received session description message. "
             "SdpParseError was: "
          << error.description;
      return;
    }
    RTC_LOG(LS_INFO) << " Received session description :" << message;
    peer_connection_->SetRemoteDescription(
        DummySetSessionDescriptionObserver::Create().get(),
        session_description.release());
    if (type == webrtc::SdpType::kOffer) {
      peer_connection_->CreateAnswer(
          this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    }
  } else {
    std::string sdp_mid;
    int sdp_mlineindex = 0;
    std::string sdp;
    if (!rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName,
                                      &sdp_mid) ||
        !rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName,
                                   &sdp_mlineindex) ||
        !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp)) {
      RTC_LOG(LS_WARNING) << "Can't parse received message.";
      return;
    }
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, &error));
    if (!candidate.get()) {
      RTC_LOG(LS_WARNING) << "Can't parse received candidate message. "
                             "SdpParseError was: "
                          << error.description;
      return;
    }
    if (!peer_connection_->AddIceCandidate(candidate.get())) {
      RTC_LOG(LS_WARNING) << "Failed to apply the received candidate";
      return;
    }
    RTC_LOG(LS_INFO) << " Received candidate :" << message;
  }
}

void Conductor::OnMessageSent(int err) {
  // Process the next pending message if any.
  main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, NULL);
}

void Conductor::OnServerConnectionFailure() {
  /*main_wnd_->MessageBox("Error", ("Failed to connect to " + server_).c_str(),
                        true);*/

  RTC_LOG(LS_INFO) << " Failed to connect" << server_.c_str();
}

//
// MainWndCallback implementation.
//

void Conductor::StartLogin(const std::string& server, int port) {
  if (client_->is_connected())
    return;
  server_ = server;
  client_->Connect(server, port, GetPeerName());
}

void Conductor::DisconnectFromServer() {
  /*if (client_->is_connected())
    client_->SignOut();*/
}

void Conductor::CreateInvate() {

  if (peer_connection_.get()) {
    main_wnd_->MessageBox(
        "Error", "We only support connecting to one peer at a time", true);
    return;
  }

  if (InitializePeerConnection()) {
    client_->user_token() = common::CrateUserToken();
    client_->room_token() = common::CrateRoomToken();
    std::string room_name = "webrtc - windows";
    peer_connection_->CreateOffer(
        this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

    

    std::map<std::string, std::string> params;
    params.insert( {"broadcaster",  client_->user_token()});
    params.insert({"roomToken", client_->room_token()});
    params.insert({"roomName", room_name});

    auto fn = [=](std::map<std::string, std::string> param_) {
      client_->Broadcast(room_name, client_->room_token(), client_->user_token());
    };

    broadcast_timer_.start(fn ,
      params, std::chrono::milliseconds(1000), [&] {
      
      });

  } else {
    main_wnd_->MessageBox("Error", "Failed to initialize PeerConnection", true);
  }
}


void Conductor::AddTracks() {
  if (!peer_connection_->GetSenders().empty()) {
    return;  // Already added tracks.
  }

  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
      peer_connection_factory_->CreateLocalMediaStream(kStreamId);

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      peer_connection_factory_->CreateAudioTrack(
          kAudioLabel,
          peer_connection_factory_->CreateAudioSource(cricket::AudioOptions())
              .get()));
  stream->AddTrack(audio_track);
  auto audios = stream->GetAudioTracks();


   auto decoder = std::unique_ptr<webrtc::VideoDecoderFactory>(
      new webrtc::MultiplexDecoderFactory(
          std::make_unique<webrtc::InternalDecoderFactory>()));

  auto videoSource = TrackSource<WindowCapturer>::Create("", {}, decoder);
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        peer_connection_factory_->CreateVideoTrack(kVideoLabel, videoSource.get()));

    stream->AddTrack(video_track);

  auto videos = stream->GetVideoTracks();

  if (!peer_connection_->AddTrack(*videos.begin(), {kStreamId}).ok()) {
    RTC_LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
  }

  if (!peer_connection_->AddTrack(*audios.begin(), {kAudioLabel}).ok()) {
    RTC_LOG(LS_ERROR) << "Adding audio to PeerConnection failed";
  }

  main_wnd_->SwitchToStreamingUI();
}

void Conductor::DisconnectFromCurrentPeer() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  if (peer_connection_.get()) {
    client_->SendHangUp(client_->user_token());
    DeletePeerConnection();
  }

  /*if (main_wnd_->IsWindow())
    main_wnd_->SwitchToPeerList(client_->peers());*/
}

void Conductor::UIThreadCallback(int msg_id, void* data) {
  switch (msg_id) {
    case PEER_CONNECTION_CLOSED:
      RTC_LOG(LS_INFO) << "PEER_CONNECTION_CLOSED";
      DeletePeerConnection();

      if (main_wnd_->IsWindow()) {
        if (client_->is_connected()) {
          //main_wnd_->SwitchToPeerList(client_->peers());
        } else {
          main_wnd_->SwitchToConnectUI();
        }
      } else {
        DisconnectFromServer();
      }
      break;

    case SEND_MESSAGE_TO_PEER: {
      RTC_LOG(LS_INFO) << "SEND_MESSAGE_TO_PEER";
      std::string* msg = reinterpret_cast<std::string*>(data);
      if (msg) {
        // For convenience, we always run the message through the queue.
        // This way we can be sure that messages are sent to the server
        // in the same order they were signaled without much hassle.
        pending_messages_.push_back(msg);
      }

      if (!pending_messages_.empty() && !client_->IsSendingMessage()) {
        msg = pending_messages_.front();
        pending_messages_.pop_front();

        if (!client_->SendToPeer(client_->user_token(), *msg) && client_->user_token().empty()) {
          RTC_LOG(LS_ERROR) << "SendToPeer failed";
          DisconnectFromServer();
        }
        delete msg;
      }

      if (!peer_connection_.get())
        client_->user_token_clear();

      break;
    }

    case NEW_TRACK_ADDED: {
      auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(data);
      if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
        main_wnd_->StartRemoteRenderer(video_track);
      }
      track->Release();
      break;
    }

    case TRACK_REMOVED: {
      // Remote peer stopped sending a track.
      auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(data);
      track->Release();
      break;
    }

    default:
      RTC_DCHECK_NOTREACHED();
      break;
  }
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
  peer_connection_->SetLocalDescription(
      DummySetSessionDescriptionObserver::Create().get(), desc);

  std::string sdp;
  desc->ToString(&sdp);

  // For loopback test. To save some connecting delay.
  if (loopback_) {
    // Replace message type from "offer" to "answer"
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp);
    peer_connection_->SetRemoteDescription(
        DummySetSessionDescriptionObserver::Create().get(),
        session_description.release());
    return;
  }

  Json::Value jmessage;
  jmessage[kSessionDescriptionTypeName] =
      webrtc::SdpTypeToString(desc->GetType());
  jmessage[kSessionDescriptionSdpName] = sdp;

  Json::StreamWriterBuilder factory;
  SendMessage(Json::writeString(factory, jmessage));
}

void Conductor::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_ERROR) << ToString(error.type()) << ": " << error.message();
}

void Conductor::SendMessage(const std::string& json_object) {
  std::string* msg = new std::string(json_object);
  main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, msg);
}

