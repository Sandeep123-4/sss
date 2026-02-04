// server.cpp
#define ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <set>
#include <string>
#include <iostream>
#include <mutex>
#include <functional>   // for std::bind, std::placeholders
#include <cstdlib>      // for getenv

typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::connection_hdl;

using namespace std;
using namespace std::placeholders;  // for _1, _2

class ChatServer {
public:
    void run(int port) {
        ws_server.init_asio();
        ws_server.set_reuse_addr(true);

        // Bind event handlers
        ws_server.set_open_handler(bind(&ChatServer::on_open, this, _1));
        ws_server.set_close_handler(bind(&ChatServer::on_close, this, _1));
        ws_server.set_message_handler(bind(&ChatServer::on_message, this, _1, _2));

        // Listen on port
        ws_server.listen(port);
        ws_server.start_accept();

        cout << "WebSocket server running on port " << port << endl;
        ws_server.run();
    }

private:
    server ws_server;
    set<connection_hdl, owner_less<connection_hdl>> clients;
    mutex mtx;

    void on_open(connection_hdl hdl) {
        lock_guard<mutex> lock(mtx);
        clients.insert(hdl);
        cout << "Client connected. Total: " << clients.size() << endl;
    }

    void on_close(connection_hdl hdl) {
        lock_guard<mutex> lock(mtx);
        clients.erase(hdl);
        cout << "Client disconnected. Total: " << clients.size() << endl;
    }

    void on_message(connection_hdl sender, server::message_ptr msg) {
        string payload = msg->get_payload();
        cout << "Received: " << payload << endl;

        lock_guard<mutex> lock(mtx);
        for (auto& client : clients) {
            if (client != sender) {               // broadcast to everyone except sender
                ws_server.send(client, payload, websocketpp::frame::opcode::text);
            }
        }
    }
};

int main() {
    // Render provides PORT via environment variable
    int port = 9002; 
    const char* env_port = getenv("PORT");
    if (env_port) port = stoi(env_port);

    ChatServer server;
    server.run(port);
}
