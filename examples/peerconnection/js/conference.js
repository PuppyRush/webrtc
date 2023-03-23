// Last updated On: January 18, 2019

// Muaz Khan      - www.MuazKhan.com
// MIT License    - www.WebRTC-Experiment.com/licence
// Documentation  - github.com/muaz-khan/WebRTC-Experiment/tree/master/Pluginfree-Screen-Sharing

var noop = true
        

var global = {
    userToken: uniqueToken()
},
channels = '--',
isGetNewRoom = true,
participants = 0,
defaultSocket = {};

function uniqueToken() {
    return Math.random().toString(36).substr(2, 35);
}

function writeLog(fn, data, fnname_)
{    

    if(noop)
    {
        if(fnname_)
        {
            console.log("[" + fnname_ + "] data:" + JSON.stringify(data));
        }
        else
        {
            fn = fn.substr('function '.length);     
            fn = fn.substr(0, fn.indexOf('('));  
            console.log("[" + fn + "] data:" + JSON.stringify(data));
        }
    }
}



var config = {
    // via: https://github.com/muaz-khan/WebRTC-Experiment/tree/master/socketio-over-nodejs
    openSocket: function(data) {
        var SIGNALING_SERVER = 'https://127.0.0.1:9559/';

        data.channel = data.channel || location.href.replace(/\/|:|#|%|\.|\[|\]/g, '');
        

        var socket = io.connect(SIGNALING_SERVER + data.channel);
        socket.channel = data.channel;
        socket.on('connected', function () {
            console.log("[connected]");

            if (data.callback) data.callback(socket);
        });
        
        socket.send = function (message) {
            console.log("[send] sender : " + sender + " data : " +  JSON.stringify(message));
            socket.emit('message', {
                sender: sender,
                data: message
            });
        };

        socket.onError = function (message) {
            console.log(message);
        };

        socket.on('message', function(data){
            
            data.onmessage(data);
        } );
                                                
    },
    onRemoteStream: function(media) {
        writeLog(arguments.callee.toString(), media, "onRemoteStream");

        if(isbroadcaster) return;

        var video = media.video;
        videosContainer.insertBefore(video, videosContainer.firstChild);
        rotateVideo(video);

        document.querySelector('.hide-after-join').style.display = 'none';
    },
    onRoomFound: function(room) {
        writeLog(arguments.callee.toString(), room, "onRoomFound");

        if(isbroadcaster) return;

        conferenceUI.joinRoom({
            roomToken: room.roomToken,
            joinUser: room.broadcaster
        });

        document.querySelector('.hide-after-join').innerHTML = '<img src="https://www.webrtc-experiment.com/images/key-press.gif" style="margint-top:10px; width:50%;" />';
    },
    onNewParticipant: function(numberOfParticipants) {
        writeLog(arguments.callee.toString(), numberOfParticipants, "onNewParticipant");

        var text = numberOfParticipants + ' users are viewing your screen!';
        
        if(numberOfParticipants <= 0) {
            text = 'No one is viewing your screen YET.';
        }
        else if(numberOfParticipants == 1) {
            text = 'Only one user is viewing your screen!';
        }

        document.title = text;
        showErrorMessage(document.title, 'green');
    },
    oniceconnectionstatechange: function(state) {
        writeLog(arguments.callee.toString(), state, "oniceconnectionstatechange");

        var text = '';

        if(state == 'closed' || state == 'disconnected') {
            text = 'One of the participants just left.';
            document.title = text;
            showErrorMessage(document.title);
        }

        if(state == 'failed') {
            text = 'Failed to bypass Firewall rules. It seems that target user did not receive your screen. Please ask him reload the page and try again.';
            document.title = text;
            showErrorMessage(document.title);
        }

        if(state == 'connected' || state == 'completed') {
            text = 'A user successfully received your screen.';
            document.title = text;
            showErrorMessage(document.title, 'green');
        }

        if(state == 'new' || state == 'checking') {
            text = 'Someone is trying to join you.';
            document.title = text;
            showErrorMessage(document.title, 'green');
        }
    }
};

function openDefaultSocket() {
    global.defaultSocket = config.openSocket({
        onmessage: defaultSocketResponse,
        callback: function(socket) {
            global.defaultSocket = socket;
        }
    });
}

var isbroadcaster = false;
var conference = function(config) {
    if (typeof adapter === 'undefined' || typeof adapter.browserDetails === 'undefined') {
        // https://webrtc.github.io/adapter/adapter-latest.js
        console.warn('adapter.js is recommended.');
    } else {
        window.adapter = {
            browserDetails: {
                browser: 'chrome'
            }
        };
    }

 
    console.log('my userToken -> ' + global.userToken  );

    var sockets = [];

 
    function defaultSocketResponse(response) {

        writeLog(arguments.callee.toString(), response);

        if (response.userToken == global.userToken) return;

        if (isGetNewRoom && response.roomToken && response.broadcaster) config.onRoomFound(response);

        if (response.newParticipant) onNewParticipant(response.newParticipant);

        if (response.userToken && response.joinUser == global.userToken && response.participant && channels.indexOf(response.userToken) == -1) {
            channels += response.userToken + '--';
            openSubSocket({
                isofferer: true,
                channel: response.channel || response.userToken,
                closeSocket: true
            });
        }
    }

    function openSubSocket(_config) {
        writeLog(arguments.callee.toString(), _config);

        if (!_config.channel) return;
        var socketConfig = {
            channel: _config.channel,
            onmessage: socketResponse,
            onopen: function() {
                if (isofferer && !peer) initPeer();
                sockets[sockets.length] = socket;
            }
        };

        socketConfig.callback = function(_socket) {
            socket = _socket;
            this.onopen();
        };

        var socket = config.openSocket(socketConfig),
            isofferer = _config.isofferer,
            gotstream,
            htmlElement = document.createElement('video'),
            inner = {},
            peer;

        var peerConfig = {
            oniceconnectionstatechange: function(p) {
                if (!isofferer) return;

                if (p && p.iceConnectionState) {
                    config.oniceconnectionstatechange(p.iceConnectionState);
                }
            },
            attachStream: config.attachStream,
            onICE: function(candidate) {
                socket && socket.send({
                    userToken: global.userToken,
                    candidate: {
                        sdpMLineIndex: candidate.sdpMLineIndex,
                        candidate: JSON.stringify(candidate.candidate)
                    }
                });
            },
            onRemoteStream: function(stream) {
                writeLog(arguments.callee.toString(), stream, "onRemoteStream");

                if (isbroadcaster) return;

                try {
                    htmlElement.setAttributeNode(document.createAttribute('autoplay'));
                    htmlElement.setAttributeNode(document.createAttribute('playsinline'));
                    htmlElement.setAttributeNode(document.createAttribute('controls'));
                } catch (e) {
                    htmlElement.setAttribute('autoplay', true);
                    htmlElement.setAttribute('playsinline', true);
                    htmlElement.setAttribute('controls', true);
                }

                htmlElement.srcObject = stream;

                _config.stream = stream;
                afterRemoteStreamStartedFlowing();
            }
        };

        function initPeer(offerSDP) {
            writeLog(arguments.callee.toString(), offerSDP);

            if (!offerSDP) peerConfig.onOfferSDP = sendsdp;
            else {
                peerConfig.offerSDP = offerSDP;
                peerConfig.onAnswerSDP = sendsdp;
            }
            peer = RTCPeerConnectionHandler(peerConfig);
        }

        function afterRemoteStreamStartedFlowing() {
            gotstream = true;

            config.onRemoteStream({
                video: htmlElement
            });

            /* closing sub-socket here on the offerer side */
            if (_config.closeSocket) socket = null;
        }

        function sendsdp(sdp) {
            writeLog(arguments.callee.toString(), sdp);

            sdp = JSON.stringify(sdp);
            var part = parseInt(sdp.length / 3);

            var firstPart = sdp.slice(0, part),
                secondPart = sdp.slice(part, sdp.length - 1),
                thirdPart = '';

            if (sdp.length > part + part) {
                secondPart = sdp.slice(part, part + part);
                thirdPart = sdp.slice(part + part, sdp.length);
            }

            socket && socket.send({
                userToken: global.userToken,
                firstPart: firstPart
            });

            socket && socket.send({
                userToken: global.userToken,
                secondPart: secondPart
            });

            socket && socket.send({
                userToken: global.userToken,
                thirdPart: thirdPart
            });
        }

        function socketResponse(response) {
            writeLog(arguments.callee.toString(), socketResponse);


            if (response.userToken == global.userToken) return;
            if (response.firstPart || response.secondPart || response.thirdPart) {
                if (response.firstPart) {
                    inner.firstPart = response.firstPart;
                    if (inner.secondPart && inner.thirdPart) selfInvoker();
                }
                if (response.secondPart) {
                    inner.secondPart = response.secondPart;
                    if (inner.firstPart && inner.thirdPart) selfInvoker();
                }

                if (response.thirdPart) {
                    inner.thirdPart = response.thirdPart;
                    if (inner.firstPart && inner.secondPart) selfInvoker();
                }
            }

            if (response.candidate && !gotstream) {
                peer && peer.addICE({
                    sdpMLineIndex: response.candidate.sdpMLineIndex,
                    candidate: JSON.parse(response.candidate.candidate)
                });
            }

            if (response.left) {
                participants--;
                if (isofferer && config.onNewParticipant) config.onNewParticipant(participants);

                if (peer && peer.peer) {
                    peer.peer.close();
                    peer.peer = null;
                }
            }
        }

        var invokedOnce = false;

        function selfInvoker() {
            writeLog(arguments.callee.toString(), socketResponse, "selfInvoker" );

            if (invokedOnce) return;

            invokedOnce = true;

            inner.sdp = JSON.parse(inner.firstPart + inner.secondPart + inner.thirdPart);
            if (isofferer && inner.sdp.type == 'answer') {
                peer.addAnswerSDP(inner.sdp);
                participants++;
                if (config.onNewParticipant) config.onNewParticipant(participants);
            } else initPeer(inner.sdp);
        }
    }

    function leave() {
        var length = sockets.length;
        for (var i = 0; i < length; i++) {
            var socket = sockets[i];
            if (socket) {
                socket.send({
                    left: true,
                    userToken: global.userToken
                });
                delete sockets[i];
            }
        }

        // if owner leaves; try to remove his room from all other users side
        if (isbroadcaster) {
            defaultSocket.send({
                left: true,
                userToken: global.userToken,
                roomToken: global.roomToken
            });
        }

        if (config.attachStream) config.attachStream.stop();
    }

    window.addEventListener('beforeunload', function() {
        leave();
    }, false);

    window.addEventListener('keyup', function(e) {
        if (e.keyCode == 116)
            leave();
    }, false);

    function startBroadcasting() {
        defaultSocket && defaultSocket.send({
            roomToken: global.roomToken,
            roomName: global.roomName,
            broadcaster: global.userToken
        });
        setTimeout(startBroadcasting, 3000);
    }

    function onNewParticipant(channel) {
        writeLog(arguments.callee.toString(), channel);

        if (!channel || channels.indexOf(channel) != -1 || channel == global.userToken) return;
        channels += channel + '--';

        var new_channel = uniqueToken();
        openSubSocket({
            channel: new_channel,
            closeSocket: true
        });

        defaultSocket.send({
            participant: true,
            userToken: global.userToken,
            joinUser: channel,
            channel: new_channel
        });
    }


    return {
        // createRoom: function(_config) {
        //     writeLog(arguments.callee.toString(), _config, "creatRoom");

        //     global.roomName = _config.roomName || 'Anonymous';
        //     global.roomToken = uniqueToken();

        //     isbroadcaster = true;
        //     isGetNewRoom = false;
        //     startBroadcasting();
        // },
        joinRoom: function(_config) {
            writeLog(arguments.callee.toString(), _config, "joinRoom");

            global.roomToken = _config.roomToken;
            isGetNewRoom = false;

            openSubSocket({
                channel: global.userToken
            });

            defaultSocket.send({
                participant: true,
                userToken: global.userToken,
                joinUser: _config.joinUser
            });
        }
    };
};

// Documentation - https://github.com/muaz-khan/WebRTC-Experiment/tree/master/RTCPeerConnection
// RTCPeerConnection-v1.5.js

var iceServers = [];

iceServers.push({
    urls:'stun:stun.l.google.com:19302',
})

// if (typeof IceServersHandler !== 'undefined') {
//     iceServers = IceServersHandler.getIceServers();
// }

// iceServers.push(
//     {urls: 'turn:14.35.66.109:5672',credential: 'chaed', username: 'chaed'});

iceServers = {
    iceServers: iceServers,
    iceTransportPolicy: 'all',
    bundlePolicy: 'max-bundle',
    iceCandidatePoolSize: 0
};

if (adapter.browserDetails.browser !== 'chrome') {
    iceServers = {
        iceServers: iceServers.iceServers
    };
}

var dontDuplicateOnAddTrack = {};

function RTCPeerConnectionHandler(options) {
    writeLog(arguments.callee.toString(), options);

    var w = window,
        PeerConnection = w.RTCPeerConnection || w.mozRTCPeerConnection || w.webkitRTCPeerConnection,
        SessionDescription = w.RTCSessionDescription || w.mozRTCSessionDescription,
        IceCandidate = w.RTCIceCandidate || w.mozRTCIceCandidate;

    var peer = new PeerConnection(iceServers);

    peer.onicecandidate = function(event) {
        writeLog(arguments.callee.toString(), event, "onicecandidate");

        if (event.candidate)
            options.onICE(event.candidate);
    };

    // attachStream = MediaStream;
    if (options.attachStream) {
        if ('addStream' in peer) {
            peer.addStream(options.attachStream);
        } else if ('addTrack' in peer) {
            options.attachStream.getTracks().forEach(function(track) {
                peer.addTrack(track, options.attachStream);
            });
        } else {
            throw new Error('WebRTC addStream/addTrack is not supported.');
        }
    }

    // attachStreams[0] = audio-stream;
    // attachStreams[1] = video-stream;
    // attachStreams[2] = screen-capturing-stream;
    if (options.attachStreams && options.attachStream.length) {
        var streams = options.attachStreams;
        for (var i = 0; i < streams.length; i++) {
            var stream = streams[i];

            if ('addStream' in peer) {
                peer.addStream(stream);
            } else if ('addTrack' in peer) {
                stream.getTracks().forEach(function(track) {
                    peer.addTrack(track, stream);
                });
            } else {
                throw new Error('WebRTC addStream/addTrack is not supported.');
            }
        }
    }

    if ('addStream' in peer) {
        peer.onaddstream = function(event) {
            writeLog(arguments.callee.toString(), event, "onaddstream");

            var remoteMediaStream = event.stream;

            // onRemoteStreamEnded(MediaStream)
            addStreamStopListener(remoteMediaStream, function() {
                if (options.onRemoteStreamEnded) options.onRemoteStreamEnded(remoteMediaStream);
            });

            // onRemoteStream(MediaStream)
            if (options.onRemoteStream) options.onRemoteStream(remoteMediaStream);

            console.debug('on:add:stream', remoteMediaStream);
        };
    } else if ('addTrack' in peer) {
        peer.ontrack = peer.onaddtrack = function(event) {
            writeLog(arguments.callee.toString(), event, "onaddtrack");

            event.stream = event.streams[event.streams.length - 1];

            if (dontDuplicateOnAddTrack[event.stream.id] && adapter.browserDetails.browser !== 'safari') return;
            dontDuplicateOnAddTrack[event.stream.id] = true;


            var remoteMediaStream = event.stream;

            // onRemoteStreamEnded(MediaStream)
            addStreamStopListener(remoteMediaStream, function() {
                if (options.onRemoteStreamEnded) options.onRemoteStreamEnded(remoteMediaStream);
            });

            // onRemoteStream(MediaStream)
            if (options.onRemoteStream) options.onRemoteStream(remoteMediaStream);

            console.debug('on:add:stream', remoteMediaStream);
        };
    } else {
        throw new Error('WebRTC addStream/addTrack is not supported.');
    }

    var sdpConstraints = {
        OfferToReceiveAudio: true,
        OfferToReceiveVideo: true
    };

    if (isbroadcaster) {
        sdpConstraints = {
            OfferToReceiveAudio: false,
            OfferToReceiveVideo: false
        };
    }

    // onOfferSDP(RTCSessionDescription)

    function createOffer() {
        writeLog(arguments.callee.toString(), event);

        if (!options.onOfferSDP) return;

        peer.createOffer(sdpConstraints).then(function(sessionDescription) {
            sessionDescription.sdp = setBandwidth(sessionDescription.sdp);
            peer.setLocalDescription(sessionDescription).then(function() {
                options.onOfferSDP(sessionDescription);
            }).catch(onSdpError);
        }).catch(onSdpError);
    }

    // onAnswerSDP(RTCSessionDescription)

    function createAnswer() {
        writeLog(arguments.callee.toString(), event);


        if (!options.onAnswerSDP) return;

        //options.offerSDP.sdp = addStereo(options.offerSDP.sdp);
        peer.setRemoteDescription(new SessionDescription(options.offerSDP)).then(function() {
            peer.createAnswer(sdpConstraints).then(function(sessionDescription) {
                sessionDescription.sdp = setBandwidth(sessionDescription.sdp);
                peer.setLocalDescription(sessionDescription).then(function() {
                    options.onAnswerSDP(sessionDescription);
                }).catch(onSdpError);
            }).catch(onSdpError);
        }).catch(onSdpError);
    }

    function setBandwidth(sdp) {
        writeLog(arguments.callee.toString(), sdp);

        if (adapter.browserDetails.browser === 'firefox') return sdp;
        if (adapter.browserDetails.browser === 'safari') return sdp;
        if (isEdge) return sdp;

        // https://github.com/muaz-khan/RTCMultiConnection/blob/master/dev/CodecsHandler.js
        if (typeof CodecsHandler !== 'undefined') {
            sdp = CodecsHandler.preferCodec(sdp, 'vp9');
        }

        // https://github.com/muaz-khan/RTCMultiConnection/blob/master/dev/BandwidthHandler.js
        if (typeof BandwidthHandler !== 'undefined') {
            window.isFirefox = adapter.browserDetails.browser === 'firefox';

            var bandwidth = {
                screen: 512, // 300kbits minimum
                video: 512 // 256kbits (both min-max)
            };
            var isScreenSharing = false;

            sdp = BandwidthHandler.setApplicationSpecificBandwidth(sdp, bandwidth, isScreenSharing);
            sdp = BandwidthHandler.setVideoBitrates(sdp, {
                min: bandwidth.video,
                max: bandwidth.video
            });
            return sdp;
        }

        // removing existing bandwidth lines
        sdp = sdp.replace(/b=AS([^\r\n]+\r\n)/g, '');

        // "300kbit/s" for screen sharing
        sdp = sdp.replace(/a=mid:video\r\n/g, 'a=mid:video\r\nb=AS:300\r\n');

        return sdp;
    }

    peer.isConnected = false;
    peer.oniceconnectionstatechange = peer.onsignalingstatechange = function() {
        if(peer && peer.isConnected && peer.iceConnectionState == 'failed') return;
        options.oniceconnectionstatechange(peer);
    };

    createOffer();
    createAnswer();

    function onSdpError(e) {
        console.error('sdp error:', JSON.stringify(e, null, '\t'));
    }

    return {
        addAnswerSDP: function(sdp) {
            writeLog(arguments.callee.toString(), sdp, "addAnswerSDP");

            console.log('setting remote description', sdp.sdp);
            peer.setRemoteDescription(new SessionDescription(sdp)).catch(onSdpError).then(function() {
                peer.isConnected = true;
            });
        },
        addICE: function(candidate) {
            writeLog(arguments.callee.toString(), candidate, "addICE");

            console.log('adding candidate', candidate.candidate);

            peer.addIceCandidate(new IceCandidate({
                sdpMLineIndex: candidate.sdpMLineIndex,
                candidate: candidate.candidate
            }));
        },

        peer: peer
    };
}

var isEdge = navigator.userAgent.indexOf('Edge') !== -1 && (!!navigator.msSaveOrOpenBlob || !!navigator.msSaveBlob);

function addStreamStopListener(stream, callback) {

    stream.addEventListener('ended', function() {
        callback();
        callback = function() {};
    }, false);
    stream.addEventListener('inactive', function() {
        callback();
        callback = function() {};
    }, false);
    stream.getTracks().forEach(function(track) {
        track.addEventListener('ended', function() {
            callback();
            callback = function() {};
        }, false);
        track.addEventListener('inactive', function() {
            callback();
            callback = function() {};
        }, false);
    });
}
