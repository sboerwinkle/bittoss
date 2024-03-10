
## Under Construction

Note that this is just for myself and some friends at the moment, so don't expect things to be pretty or well documented. Also it's only written for Linux. It may never move past this stage, but I'm having fun with it, and that's what counts.

You should be able to build the client with `build.sh`. It requires GLFW3, and some other stuff. The server resides in `server/` and uses python3. Both client and server can be run without args to see usage information.

Once in-game, the controls are WASD, mouse (look/LMB/RMB), Space, Tab, and Left Shift. The default map is sort of capture-the-flag, though there's not proper scoring. There's _very_ limited text chat (T), but it's more fun if you're on an external voice call.

Documentation for the in-game editing functionality (and other advanced commands) resides in the `docs/` folder.

There's some other stuff as well, but I'm not going to put effort into writing that all down when it still might change.

*Contributors:*

sboerwinkle: Yours truly, primary author

mboerwinkle:

- Starter graphics code, as well as some graphical QoL improvements
- IPv6 support, server hardening, and host resolution for networking

pboerwinkle: Designed `itm/statue.itm`
