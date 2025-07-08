# Space Station Manager

This repository contains a prototype framework for a 2D top-down game using an authoritative server model.

* **Server** – implemented with `boost::asio` to handle clients and maintain the world state.
* **Client** – uses OpenGL through GLFW and GLEW for rendering.
* **Map** – infinite grid of cells. The starting area is a 20x20 walkable space station surrounded by empty space.

## Building

```bash
mkdir build && cd build
cmake ..
make
```

This produces two executables, `server` and `client`.

## Controls

Run `server` and then `client`. In the client window use:

- `W`, `A`, `S`, `D` to move your character around the 20x20 station.
- `E` to toggle doors when standing next to them.
