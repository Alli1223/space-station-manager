#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <sstream>
#include "Map.h"

using boost::asio::ip::tcp;

struct Player {
    int x = 10;
    int y = 10;
    bool edit = false;
};

std::string handle_command(char cmd, Player& player, GameMap& map) {
    int nx = player.x;
    int ny = player.y;
    bool changed = false;
    int cx = 0, cy = 0; // changed cell coords
    Cell::Type newState = Cell::Empty;

    if (cmd == 'P') {
        player.edit = !player.edit;
    } else if (cmd == 'W') {
        ny -= 1;
    } else if (cmd == 'S') {
        ny += 1;
    } else if (cmd == 'A') {
        nx -= 1;
    } else if (cmd == 'D') {
        nx += 1;
    } else if (cmd == 'E' && !player.edit) {
        Point dirs[4] = { {player.x+1, player.y}, {player.x-1, player.y},
                           {player.x, player.y+1}, {player.x, player.y-1} };
        for (auto& p : dirs) {
            auto cell = map.get(p.x, p.y).type;
            if (cell == Cell::DoorClosed) {
                map.set(p.x, p.y, Cell::DoorOpen);
                changed = true; cx = p.x; cy = p.y; newState = Cell::DoorOpen; break;
            } else if (cell == Cell::DoorOpen) {
                map.set(p.x, p.y, Cell::DoorClosed);
                changed = true; cx = p.x; cy = p.y; newState = Cell::DoorClosed; break;
            }
        }
    } else if (player.edit && (cmd == 'B' || cmd == 'V' || cmd == 'N')) {
        Cell::Type t = Cell::Walkable;
        if (cmd == 'B') t = Cell::Wall;
        else if (cmd == 'N') t = Cell::DoorClosed;
        else if (cmd == 'V') t = Cell::Walkable;
        map.set(player.x, player.y, t);
        changed = true; cx = player.x; cy = player.y; newState = t;
    }

    if (player.edit || Cell::isWalkable(map.get(nx, ny).type)) {
        player.x = nx; player.y = ny;
    }

    std::ostringstream oss;
    oss << "POS " << player.x << ' ' << player.y;
    if (changed) {
        oss << " SET " << cx << ' ' << cy << ' ' << Cell::toString(newState);
    }
    oss << '\n';
    return oss.str();
}

void session(tcp::socket socket, GameMap& map) {
    try {
        Player player;
        for (;;) {
            char c;
            boost::system::error_code ec;
            size_t n = boost::asio::read(socket, boost::asio::buffer(&c,1), ec);
            if (ec || n == 0) break;
            std::string response = handle_command(c, player, map);
            boost::asio::write(socket, boost::asio::buffer(response), ec);
            if (ec) break;
        }
    } catch (std::exception& e) {
        std::cerr << "session error: " << e.what() << std::endl;
    }
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 12345));
        GameMap map;
        for (;;) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            std::thread(session, std::move(socket), std::ref(map)).detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "Server exception: " << e.what() << std::endl;
    }
}
