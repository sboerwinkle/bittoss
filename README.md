
## Under Construction

Note that this is just for myself and some friends at the moment, so don't expect things to be pretty or well documented. Also it's only written for Linux. It may never move past this stage, but I'm having fun with it, and that's what counts.

The server resides in `server/` and uses python3. It does not need any arguments.

You should be able to build the client with `build.sh`. It requires GLFW3, and some other stuff. It needs one argument, the server address (e.g. `localhost`).

Once in-game, the default map is sort of capture-the-flag, though there's not proper scoring. There's _very_ limited text chat (T), but it's more fun if you're on an external voice call. Controls are something like:

- WASD: move
- Space: jump
- Mouse: look, primary/secondary fire
- Left Shift: hold while bumping into equipment to pick it up. Shift + click drops equipment. The only "equipment" on the default map is the flags.
- Tab: toggles first-person, which used to be a lot more useful

You can change your nametag (visible to other players) with `/name`.

Documentation for the in-game editing functionality (and other advanced commands) resides in the `docs/` folder.

There's some other stuff as well, but I'm not going to put effort into writing that all down when it still might change.

*Contributors:*

sboerwinkle: Yours truly, primary author

mboerwinkle:

- Starter graphics code, as well as some graphical QoL improvements
- IPv6 support, server hardening, and host resolution for networking

pboerwinkle: Designed `itm/statue.itm`
