// WebSocket to TCP proxy
// Bridges browser WebSocket clients to native TCP VOLE server

const WebSocket = require('ws');
const net = require('net');

const WS_PORT = 8080;
const TCP_HOST = '127.0.0.1';
const TCP_PORT = 12345;

const wss = new WebSocket.Server({ port: WS_PORT });

console.log(`WebSocket proxy listening on ws://localhost:${WS_PORT}`);
console.log(`Will connect to TCP server at ${TCP_HOST}:${TCP_PORT}`);

wss.on('connection', function connection(ws) {
    console.log('Browser client connected via WebSocket');

    // Connect to native TCP server
    const tcpSocket = net.createConnection({ host: TCP_HOST, port: TCP_PORT }, () => {
        console.log('Connected to native VOLE server');
    });

    tcpSocket.on('error', (err) => {
        console.error('TCP connection error:', err.message);
        ws.close();
    });

    // Forward TCP data to WebSocket
    tcpSocket.on('data', (data) => {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(data);
        }
    });

    tcpSocket.on('close', () => {
        console.log('TCP connection closed');
        ws.close();
    });

    // Forward WebSocket data to TCP
    ws.on('message', (data) => {
        tcpSocket.write(data);
    });

    ws.on('close', () => {
        console.log('WebSocket connection closed');
        tcpSocket.end();
    });

    ws.on('error', (err) => {
        console.error('WebSocket error:', err.message);
        tcpSocket.end();
    });
});
