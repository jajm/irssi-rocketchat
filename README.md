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
* list public channels
* list users
* join an existing room (public/private channel, discussion, direct message).
  leaving a room is not implemented
* receive/send messages from/to rooms
* automatic join to rooms when a message is received
* room history: the last 10 messages are loaded when joining a room
* nicklist

## TODO

* Use queries when possible. For now every room is a channel, even direct message between two users.
* Allow to use the name of the room instead of its ID when joining

## Installation

### Requirements

* irssi development files
* [glib](https://developer.gnome.org/glib/)
* [Jansson](https://digip.org/jansson/) (>= 2.11)
* [libwebsockets](https://libwebsockets.org/) (>= 4.1) with glib support

For irssi development files, glib and jansson you can use your package manager.

```sh
apt-get install irssi-dev libglib2.0-dev libjansson-dev
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

Once connected you can list public channels

```
/rocketchat channels
```

```
foo (ID: aQhQEXsMuoS439dGS)
general (ID: GENERAL)
test (ID: 497BsckNzoaT2PeMF)
```

and join one or more using their ID

```
/join aQhQEXsMuoS439dGS,GENERAL,497BsckNzoaT2PeMF
```

To automatically join a room when connected to the server you can add it to
your channels list

```
/channel add -auto 497BsckNzoaT2PeMF MyRocketChat
```

[Rocket.Chat]: https://rocket.chat/
