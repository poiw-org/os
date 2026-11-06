```
      .=+****+.   .-=====:
     ,***+*-+#-  :+++==++*=
     =*+++*-=#===##-+**+++*
     =##%%##+=+**#+++*++++#
      =%%**%%#%%####++++*#*            ,,.
      +@*+%%@@@*=*%%%****+,          :=+*+-
      -#%%%###%##%%%#**#=:          -**--=*+=-,
      .-##+=-+*#%%##+=+***+;.       +#=. ,**#*+-
      ,:##---+*#%#+++-==+*#%*=.     =*:  ,+**##*;
  ,.::-+##=-+#%@###+=---=++*%%*=    =#=:  ,;+##*+-
.:::::-=+%+#%%+#%+#+*-----=+++%%-   -**-    :;++++
..  ;-;;**=*-+;=%*=-=+------=+%@%*,  -**-     ..
    ;;;=;;;,.==+*+#*--------==+#%%*- .-+*+-
    .:-;    .-#*--++=-------+#*+%%*#*: .-+*+-,
    ..=+*,  ;*#=-------------#%=+@##%#-  ,=##=,
    --##%=-+##=--------------=%=+@#+*%#=   ,%#=
   .#=#%*%*#%+----------------%++@#++#%+    ##*
   .+#**%#+%#---------------=##**%#++*%#,   *##,
    -==++;.%#---------------*%*##%*+++*%-   ###,
           ##=--------------+*#%%#++++*%- ;*%##.
           +%*===============+++++++++*@==#%#%+
            ##++++++++++++++++++++++++#@*%##%+.
            ,#%++++++++++++++++++++++#@%%%#*.
          ;*+*#%##*###**##***++++*#%%#++==;
          ;***+*####*=--==++#%%%%##%%*
            ,:-==-,         ==-=====+-
```

# Doora

Doora is the door lock of the po/iw hackerspace. You tap an NFC tag on the reader, or press unlock in the web app, and the door opens. The repo has two parts: the firmware that runs on the door, and the po/iw web app, which is both the hackerspace's front page and the place where access is managed.

The door makes its own decisions. A key is a signed token. The door checks the signature, the validity dates and a list of disabled keys without asking a server, so tags keep working when the Wi-Fi or internet is down. When it is online it syncs the disabled-key list and uploads its log of taps.

## What's in the repo

| Folder | What it is |
| --- | --- |
| [`doora/`](doora) | ESP32 firmware in C++, built with PlatformIO. Reads tags with a PN532, drives the relay, buzzer and LEDs, and talks to Supabase. |
| [`web/`](web) | The po/iw operating system: the hackerspace's front page plus the members' and admins' app behind the login, with the Supabase migrations and edge functions that it and the door use. Access control is its first feature, and future IoT devices are meant to plug in here too. |

## How the parts fit together

- Admins sign in to the web app and issue keys to users. The `keys` edge function signs the token. It is then either written to a tag through the door (which has a card writer mode) or kept as a virtual key with no tag.
- Members see their own keys in the web app and can open the door from it. That request goes to the `unlock` edge function, which checks the member is allowed, sends the command to the door over a Supabase realtime channel, and logs the open.
- The door signs in to Supabase as its own user with very limited rights. It fetches the public key and the disabled-key list, listens for live changes and force-open commands, and uploads its journal.
- Users, keys, the door log and the activity feed live in Postgres on Supabase, protected by row level security.

## Getting started

Each folder has its own README with the full steps.

- Firmware: copy `doora/config.example.h` to `doora/config.h`, set your pins, run `make upload`, then answer the setup questions on the serial console. See [doora/README.md](doora/README.md).
- Web app: run `pnpm install` and `pnpm dev` inside `web/`, with a Supabase project behind it. See [web/README.md](web/README.md).

Made by the po/iw hackerspace.
