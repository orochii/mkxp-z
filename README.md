# mkxp

mkxp is a project that seeks to provide a fully open source implementation of the Ruby Game Scripting System (RGSS) interface used in the popular game creation software "RPG Maker XP" (trademark by Enterbrain, Inc.), with focus on Linux. The goal is to be able to run games created with the above software natively without changing a single file.

It is licensed under the GNU General Public License v2.

## Bindings
Bindings provide the interpreted language environment to run game scripts in. As of right now, they are compiled directly into the executable. Currently there are three bindings:

### MRI
Website: https://www.ruby-lang.org/en/

Matz's Ruby Interpreter, also called CRuby, is the most widely deployed version of ruby. If you're interested in running games created with RPG Maker XP, this is the one you should go for. MRI 1.8 is what was used in RPG Maker XP, however, this binding is written against 2.0 (the latest version). For games utilizing only the default scripts provided by Enterbrain, this binding works quite well so far. Note that there are language and syntax differences between 1.8 and 2.0, so some user created scripts may not work correctly.

For a list of differences, see:
http://stackoverflow.com/questions/21574/what-is-the-difference-between-ruby-1-8-and-ruby-1-9

To select this backend, run `qmake BINDING=BINDING_MRI`

### mruby (Lightweight Ruby)
Website: https://github.com/mruby/mruby

mruby is a new endeavor by Matz and others to create a more lightweight, spec-adhering, embeddable Ruby implementation. You can think of it as a Ruby version of Lua.

Due to heavy differences between mruby and MRI as well as lacking modules, running RPG Maker games with this backend will most likely not work correctly. It is provided as experimental code. You can eg. write your own ruby scripts and run them with this backend.

Some extensions to the standard classes/modules are provided taking the RPG Maker XP helpfile as a quasi "standard". These include Marshal, File, FileTest and Time.

To select this backend, run `qmake BINDING=BINDING_MRUBY`

### null
This backend only exists for testing purposes and does nothing (the engine quits immediately).

To select this backend, run `qmake BINDING=BINDING_NULL`

## Dependencies

* QtCore 4.8
* libsigc++
* PhysFS
* glew
* SDL2
* SDL2_image
* SDL2_ttf
* sfml-system 2.0
* sfml-audio 2.0

(If no version specified, assume latest)
 
### MRI binding:
Place a recent version of ruby in the project folder and build it.

### mruby binding:
Place a recent version of mruby in the project folder and build it.

To run mkxp, you should have a graphics card capable of at least **OpenGL 3.0**

## Building

mkxp employs Qt's qmake build system, so you'll need to install that beforehand. After cloning mkxp, run one of the above qmake calls, or simply `qmake` to select the default backend (currently MRI), then `make`.

## Configuration

mkxp reads configuration data from the file "mkxp.conf" contained in the current directory. The format is ini-style.

* "gameFolder": Specifies where mkxp will look for the game scripts. Default is the current directory.
* "customScript": Specifies a raw ruby script file to be run instead of an RPG Maker game, residing in "gameFolder".
* "RTPs": Specifies a list of space separated paths to RTPs to be used. (See next section)

Most other entries are self explanatory.

## RTPs

As of right now, mkxp doesn't support midi files, so to use the default RTPs provided by Enterbrain you will have to convert all midi tracks (those in BGM and ME) to ogg or wav. Make sure that the file names match up, ie. "foobar.mid" should be converted to "foobar.ogg".

## Fonts

In the RMXP version of RGSS, fonts are loaded directly from system specific search paths (meaning they must be installed to be available to games). Because this whole thing is a giant platform-dependent headache, I decided to implement the behavior Enterbrain thankfully added in VX Ace: loading fonts will automatically search a folder called "Fonts", which obeys the default searchpath behavior (ie. it can be located directly in the game folder, or an RTP).

If a requested font is not found, no error is generated. Instead, a built-in font is used (currently "Liberation Sans").

## What doesn't work

* Audio formats other than ogg/wav (this might change in the future)
* Audio "pitch" parameter
* The Win32API ruby class (for obvious reasons)
* Loading Bitmaps with sizes greater than the OpenGL texture size limit (around 8192 on modern cards)