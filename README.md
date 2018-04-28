# Dig-Company
It is a company who solves the problem of MD5 RUSH in cooperation! Though this problem does not exist a polynomial time solution, we can still employ lots of miners to help us solve the problem more efficiently!
## Rule to MD5 RUSH
A mine is an arbitrary binary string. Here is an example in C format string.
```
"\x31\x30\x33"
```
A 1-treasure is a binary string which MD5 hash in hex is prefixed with exactly 1 zero. For example, ``\x31\x30\x33\x33\x30`` is an 1-treasure.
```
MD5("\x31\x30\x33\x33\x30") == 018dbfb5fec8d864714ede49cef50343
```
By appending arbitrary long suffix to 1-treasure, we obtain a 2-treasure. It must be prefixed with an 1-treasure, and of course, its MD5 hash is prefixed with exactly 2 zeros. In general, an (n+1)-treasure is prefixed with an n-treasure.
From the example above, we can append ``\x31\x30\x35\x37\x39`` to turn it into a 2-treasure.
```
MD5("\x31\x30\x33\x33\x30\x31\x30\x35\x37\x39") == 003862fe4f1610856a8b3e63bc3608e6
```
Here comes the the challenge: Let’s start with you student ID in upper case as your mine, say R06922148 for instance. By appending suffix to your mine, try to find out an n-treasure with n as large as possible.
```
MD5("R06922148" + "\xOO\xOO\xOO...") == 0000000?????????????????????????
```
## Usage
### STEP 1 - Configure config/config.txt
```
vim config/config.txt
```
The config file has fields of two types: one MINE and several MINER fields.  
* MINE: A file storing the binary string, the mine. The server will try to append some bytes to it to find out treasures.
* MINER: A input/output named pipe pair corresponding to a specific MINER process. The paths are seperated by a single space, and paths are guaranteed to include no spaces.
```
MINE: /tmp/mine.bin\n
MINER: /tmp/fifo1_in /tmp/fifo1_out\n
MINER: /tmp/fifo2_in /tmp/fifo2_out\n
...
MINER: [Path/to/input/pipe] [Path/to/output/pipe]
```
You can set up arbitrary numbers of client by configuring lots of miner here!
### STEP 2 - Start up client
```
cd client
make
./miner [miner's name] [input pipe] [output pipe]
```
For example
```
# terminal 1
./miner Ada /tmp/ada_in /tmp/ada_out

# terminal 2
./miner Margaret /tmp/margaret_in /tmp/margaret_out

# terminal 3
./miner Hopper /tmp/hopper_in /tmp/hopper_out
...
# terminal [Number of miners]
```
### STEP 3 - Start up server
```
cd server
make
./boss ../config/config.txt
```
### More detailed SPEC
```
https://systemprogrammingatntu.github.io/MP3
```
