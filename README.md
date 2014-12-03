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
`pulltab` is licensed under the MIT/X11 License. See the [LICENSE](LICENSE) file
for more information.
