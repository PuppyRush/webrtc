// https://www.webrtc-experiment.com/


var product = require('./controller')

// import os module
const os = require("os");

// check the available memory
const userHomeDir = os.homedir();

var fs = require('fs');

// don't forget to use your own keys!
var options = {
    key: fs.readFileSync(__dirname + "/fake-keys/privatekey.pem"),
    cert: fs.readFileSync(__dirname + "/fake-keys/certificate.pem")
};


var winPeerSock;
var webPeerSock;
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
// HTTPs server
var httpsServer = require('https').createServer( options, function(req, response) {

    const { headers } = req;
    const userAgent = headers['user-agent'];

    // console.log("somebody comming... : " + req.url +","+ headers["content-type"] + ","+ headers["content-length"] + ",", headers["content-encoding"] ,
    // ",", headers["user-agent"]);

    // winPeerSock = response.socket; 
    // io.emit("send","GET /wait?peer_id=3 HTTP/1.0\r\n\r\n");
    // winPeerSock.emit('event', "GET /wait?peer_id=3 HTTP/1.0\r\n\r\n", 12,34);

    // response.writeHead(200, {
    //     'Content-Type': 'application/json',
    //     'Content-Length' : 3
    //   });
    // response.write('dsd');
    // response.end();




    // if(userAgent?.match("windows10"))
    // {
    //     winPeerSock = response.socket;
    //     io.emit("send","GET /wait?peer_id=3 HTTP/1.0\r\n\r\n");
    //     winPeerSock.emit('event', "GET /wait?peer_id=3 HTTP/1.0\r\n\r\n", 12,34);
    // }
    // else{
    //     webPeerSock = req.socket;
    //     io.emit("send","GET /wait?peer_id=3 HTTP/1.0\r\n\r\n");
    //     webPeerSock.emit('event', "GET /wait?peer_id=3 HTTP/1.0\r\n\r\n", 12,34);
    // }

    if(req.url == '/')
    {
        response.writeHead(200, {
            'Content-Type': 'text/html'
        });
        var link = 'https://github.com/muaz-khan/WebRTC-Experiment/tree/master/websocket-over-nodejs';
        response.write('<title>websocket-over-nodejs</title><h1><a href="'+ link + '">websocket-over-nodejs</a></h1><pre>var websocket = new WebSocket("wss://webrtcweb.com:9449/");</pre>');
        response.end();
    }
    else if(req.url == '/streaming'){
        const path_ = __dirname +'/index.html';
        fs.createReadStream(path_).pipe(response);
    }
    else if(req.url == '/conference.js'){
        const path_ = __dirname +'/conference.js';
        fs.createReadStream(path_).pipe(response);
    }
}).listen(process.env.PORT || 9559);




// socket.io goes below
const { Server } = require("socket.io");
const io = new Server(httpsServer, { 
    log: true,
    origins: '*:*',
});

  
// io.set('transports', [
//     //'websocket',
//     'xhr-polling',
//     'jsonp-polling'
// ]);


var channels = {};

io.sockets.on('connection', function (socket) {
    
     console.log('io.sockets.on(\'connection\')');
    socket.emit('message', 'for-test');

    var initiatorChannel = '';
    if (!io.isConnected) {
        io.isConnected = true;
    }

    socket.on('new-channel', function (data, callback) {
        if (!channels[data.channel]) {
            initiatorChannel = data.channel;
            console.log('[new-channel] new channel : ' + data.channel + " data sender:" + data.sender +" data:" + data.data);
        }
        else
        {
            console.log('[new-channel] channel : ' + data.channel + " data sender:" + data.sender +" data:" + data.data);
        }

        channels[data.channel] = data.channel;
        onNewNamespace(data.channel, data.sender);

        callback("new-channel end");
    });

    socket.on('presence', function (channel) {
        var isChannelPresent = !! channels[channel];
        socket.emit('presence', isChannelPresent);
    });

    socket.on('disconnect', function (channel) {
        if (initiatorChannel) {
            delete channels[initiatorChannel];
        }
    });
});

function onNewNamespace(channel, sender) {
    io.of('/' + channel).on('connection', function (socket) {

        console.log('onNewNamespace channel:' +channel + ", sender:" + sender);

        var username;
        if (io.isConnected) {
            io.isConnected = false;
            socket.emit('connected', true);
        }

        socket.on('message', function (data) {
            console.log('message:' + data.data + ", data sender:" + data.sender);

            if (data.sender == sender) 
            {
                if(!username) username = data.sender;
                
                socket.broadcast.emit('message', data.data);

                
            }
        });
        
        socket.on('disconnect', function() {
            if(username) {
                socket.broadcast.emit('user-left', username);
                username = null;
            }
        });

        
    });
}

// run app

io.engine.on("connection_error", (err) => {
    console.log(err.req);      // the request object
    console.log(err.code);     // the error code, for example 1
    console.log(err.message);  // the error message, for example "Session ID unknown"
    console.log(err.context);  // some additional error context
  });


process.on('unhandledRejection', (reason, promise) => {
  process.exit(1);
});


console.log('Please open SSL URL: https://localhost:'+(process.env.PORT || 9559)+'/');
