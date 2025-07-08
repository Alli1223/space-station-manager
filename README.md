# Space Station Manager

This repository contains a prototype framework for a 2D top-down game using an authoritative server model.

* **Server** – implemented with `boost::asio` to handle clients and maintain the world state.
* **Client** – uses OpenGL through GLFW and GLEW for rendering.
* **Map** – infinite grid of cells. The starting area is a 20x20 space station
  enclosed by impassable walls with empty space outside. The server persists the
  current map to `map.txt` so edits are kept between runs.

## Building

```bash
mkdir build && cd build
cmake ..
make
```

This produces two executables, `server` and `client`.

You can also use the provided Makefile:

```bash
make run-server   # build and run only the server
make run-client   # build and run only the client
make run          # start the server in the background and the client in the foreground
```

## Controls

Run `server` and then `client`. In the client window use:

- `W`, `A`, `S`, `D` to move your character.
- `E` to toggle doors when next to them.
- `P` to toggle Station Edit mode.
- When editing: `B` places a wall, `V` places floor, `N` places a closed door at your position.

When the server shuts down (for example with `Ctrl+C`), the map is written to `map.txt`. Restarting the server loads this file so your station layout persists across runs.
