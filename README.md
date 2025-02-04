
## Under Construction

Note that this is just for myself and some friends at the moment, so don't expect things to be pretty or well documented. It's intended to run on Linux, but may work on Windows using WSL (see below). It may never move past this stage, but I'm having fun with it, and that's what counts.

The server resides in `server/` and uses python3. It does not need any arguments.

You should be able to build the client with `build.sh`. It requires GLFW3, `pkg-config`, and maybe some other stuff.
The compiled binary (`./game`) needs one argument, the server address (e.g. `localhost`).

The starting map is a sort of lobby. You can pick teams, experiment with equipment, and load any of the listed levels by touching the labeled spike. You can reset to this lobby at any time by typing `/load lobby.sav`. There's _very_ limited text chat (T), but it's more fun if you're on an external voice call. Controls are something like:

- WASD: move
- Space: jump
- Mouse: look, primary/secondary fire
- Left Shift: Shift + click drops equipment.
- Tab: toggles first-person, which used to be a lot more useful

You can change your nametag (visible to other players) with `/name`.

Documentation for the in-game editing functionality (and other advanced commands) resides in the `docs/` folder.

There's some other stuff as well, but I'm not going to put effort into writing that all down when it still might change.

## Running on Windows

The setup here is admittedly a little rough around the edges. A proper port to Windows should be feasible, given that GLFW3 already supports Windows, but I haven't tried yet.

To run on Windows, set up WSL, then download, build, and run the game as you would on Linux.

At this point it should be able to launch, but the mouse behavior will be all weird because WSL hasn't solved mouse capturing with OpenGL apps (tracked [here](https://github.com/microsoft/wslg/issues/376)). This can be solved with VcXsrv; install that, then put `docs/bittoss_vcxsrv.bat` on your desktop or something to launch it.

## Contributors

sboerwinkle: Yours truly, primary author

mboerwinkle:

- Starter graphics code, as well as some graphical QoL improvements
- IPv6 support, server hardening, and host resolution for networking

pboerwinkle: Designed `itm/statue.itm`
