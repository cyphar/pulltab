### pulltab ###
A complete rewrite of `corkscrew`, with much better code cleanliness and
readability (and memory management is far more stringently taken care of).

#### Usage ####
```
pulltab [-a <auth-file>] -x proxy[:port] -d dest[:port] [-h]
Tunnel arbitrary streams through HTTP proxies.

Options:
   -a <auth-file>  -- use HTTP Basic authentication, with the credentials in the given file (of the form 'user\x00pass').
   -x proxy[:port] -- tunnel through the given HTTP proxy (default port is 8080).
   -d dest[:port]  -- tunnel through to the given destination address (default port is 22).
   -h              -- print this help page and exit.
```

A common method of using `-a` is to just use process substitution:
```bash
$ pulltab -x <proxy>:<port> -d <dest>:<port> -a <(printf "user\0password")
```

The reason why an auth *file* is used, rather than just allowing users to pass
the username and password as an argument, is because arguments can be seen by
all other users on a system (by accessing `/proc/<pid>/cmdline`).

#### Compatibility ####
`pulltab` (to my knowledge) works with all proxy servers I've tested it with:

* Squid
* Polipo
* BlueCoat

If you have tested `pulltab` with any other proxy servers and found that it
works on those too, please tell me so I can add it to the above list.

#### License ####

([GPLv3 or later](https://www.gnu.org/licenses/gpl-3.0.en.html))

```
Copyright (C) 2014 Aleksa Sarai

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
```
