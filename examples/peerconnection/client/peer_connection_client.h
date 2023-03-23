/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_PEERCONNECTION_CLIENT_PEER_CONNECTION_CLIENT_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_PEER_CONNECTION_CLIENT_H_

#include <set>
#include <memory>
#include <string>

#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/socket_io.h"

typedef std::set<std::string> Peers;

enum class message_type {
	connected,
	sign_out,
	message,
};

struct PeerConnectionClientObserver {
	virtual void StartInvate() = 0;  // Called when we're logged on.
	virtual void OnDisconnected() = 0;
	virtual void OnPeerConnected(std::string userToken, const std::string& name) = 0;
	virtual void OnPeerDisconnected(std::string userToken) = 0;
	virtual void OnMessageFromPeer(std::string userToken, const std::string& message) = 0;
	virtual void OnMessageSent(int err) = 0;
	virtual void OnServerConnectionFailure() = 0;

protected:
	virtual ~PeerConnectionClientObserver() {}
};

class PeerConnectionClient
	: public sigslot::has_slots<>
	{
public:
	enum State {
		NOT_CONNECTED,
		RESOLVING,
		SIGNING_IN,
		CONNECTED,
		SIGNING_OUT_WAITING,
		SIGNING_OUT,
	};

	inline static PeerConnectionClient& instance()
	{
		static PeerConnectionClient client;
		return client;
	}

	std::string user_token() const;
	void user_token_clear();

	std::string room_token() const;
	void room_token_clear();
	
	bool is_connected() const;
	Peers& peers();

	std::string MessageTypeName(message_type type);

	void RegisterObserver(PeerConnectionClientObserver* callback);

	void Connect(const std::string& server,
		int port,
		const std::string& client_name);

	std::string MakeResponseString(message_type type, std::string user_token, std::string msg);

	bool SendToPeer(std::string user_token, const std::string& message);
	bool SendHangUp(std::string user_token);
	bool IsSendingMessage();

	//bool SignOut();

	void Broadcast(const std::string room_name, const std::string room_token, const std::string user_token);
protected:

	PeerConnectionClient();
	~PeerConnectionClient();
	
	void DoConnect();
	void Close();
	void InitSocketSignals();
	bool ConnectControlSocket();
	void OnConnect(rtc::Socket* socket);
	void OnHangingGetConnect(rtc::Socket* socket);
	void OnMessageFromPeer(std::string userToken, const std::string& message);

	// Quick and dirty support for parsing HTTP header values.
	/*bool GetHeaderValue(const std::string& data,
		size_t eoh,
		const char* header_pattern,
		size_t* value);

	bool GetHeaderValue(const std::string& data,
		size_t eoh,
		const char* header_pattern,
		std::string* value);*/

	// Returns true if the whole response has been read.
	bool ReadIntoBuffer(rtc::Socket* socket,
		std::string* data,
		size_t* content_length);

	void OnRead(rtc::Socket* socket);

	void OnHangingGetRead(rtc::Socket* socket);

	// Parses a single line entry in the form "<name>,<id>,<connected>"
	bool ParseEntry(const std::string& entry,
		std::string* name,
		std::string& id,
		bool* connected);

	int GetResponseStatus(const std::string& response);

	/*bool ParseServerResponse(const std::string& response,
		size_t content_length,
		std::string& response& peer_id,
		size_t* eoh);*/

	void OnClose(rtc::Socket* socket, int err);

	void OnResolveResult(rtc::AsyncResolverInterface* resolver);

	

private:

	PeerConnectionClientObserver* callback_;
	rtc::SocketAddress server_address_;
	rtc::AsyncResolver* resolver_;
	std::unique_ptr<rtc::Socket> control_socket_;
	std::unique_ptr<rtc::Socket> hanging_get_;
	std::string onconnect_data_;
	std::string control_data_;
	std::string notification_data_;
	std::string client_name_;
	std::string room_token_;
	std::string channel_;
	Peers peers_;
	State state_;
	std::string user_token_;
	webrtc::ScopedTaskSafety safety_;


};

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_PEER_CONNECTION_CLIENT_H_
