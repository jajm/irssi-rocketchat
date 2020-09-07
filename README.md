# irssi-rocketchat

irssi-rocketchat is an irssi module to connect to a [Rocket.Chat] instance

It is still in a work-in-progress state and a lot of things does not work, so
use it at your own risks.

## Why ?

Because I like irssi and I don't like using a mouse for a text chat, nor
waiting 2 seconds to autocomplete a nick

## What works ?

* login using an authentication token (you need to log in using a web browser
  to generate a token)
* receive/send messages from/to rooms that are already opened (you will need a
  web browser to join/leave rooms)

## Installation

### Requirements

* [glib]
* [Jansson] (>= 2.11)
* [libwebsockets] (>= 4.1) with glib support

For glib and jansson you can use your package manager.

```sh
apt-get install libglib2.0-dev libjansson-dev
```

But you will need to build libwebsockets from source.

Install cmake first if you don't have it.
```
apt-get install cmake
```

Then proceed to build libwebsockets.

```sh
git clone -b v4.1-stable https://libwebsockets.org/repo/libwebsockets
cd libwebsockets
mkdir build && cd build
cmake -DLWS_WITH_GLIB=1 .. && make
# as root
make install
```

### Building

Once all dependencies are installed, build and install irssi-rocketchat

```sh
cd /path/to/irssi-rocketchat
make && make install
```

This will install irssi-rocketchat in ~/.irssi/modules

## Usage

First generate an access token:

* sign in with a web browser to your Rocket.Chat instance
* on your account page, go to "Personal access tokens"
* create a new access token and keep it visible, you will need to copy it into
  your irssi config file

Then define a new chatnet and a new server in your ~/.irssi/config

```
chatnets = {
    MyRocketChat = {
        type = "rocketchat";
    }
);
servers = (
    {
        address = "chat.example.com";
        chatnet = "MyRocketChat";
        port = "443";
        use_tls = "yes";
        password "<your-access-token>";
    }
);
```

Then open irssi and enter the following commands:

```
/load rocketchat
/connect MyRocketChat
```

Several windows should open, you can now start chatting!

[Rocket.Chat]: https://rocket.chat/
[glib]: https://developer.gnome.org/glib/
[Jansson]: https://digip.org/jansson/
[libwebsockets]: https://libwebsockets.org/
