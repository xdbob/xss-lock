========
xss-lock
========

-------------------------------------
use external locker as X screen saver
-------------------------------------

:Author: Raymond Wagenmaker <raymondwagenmaker@gmail.com>
:Date: April 2013
:Manual section: 1

Synopsis
========

| xss-lock [-n *notify_cmd*] [--ignore-sleep] [-v|-q] ... [--] *lock_cmd* [*arg*] ...
| xss-lock --help|--version

Description
===========

**xss-lock** hooks up your favorite locker to the MIT screen saver extension
for X and also to systemd's login manager. The *lock_cmd* is executed in
response to events from these two sources:

- X signals when screen saver activation is forced or after a period of user
  inactivity (as set with ``xset s TIMEOUT``). In the latter case, the notifier
  command, if specified, is executed first.

- The login manager can also request that the session be locked; as a result of
  ``loginctl lock-sessions``, for example. Additionally, **xss-lock** uses the
  inhibition logic to lock the screen before the computer goes to sleep.

**xss-lock** waits for the locker to exit -- or kills it when screen saver
deactivation or session unlocking is forced -- so the command should not fork.

Options
=======

-n cmd, --notifier=cmd
                Run *cmd* when the screen saver activates due to user
                inactivity. The notifier is killed when X signals user activity
                or when the locker is started. The locker is started after the
                first screen saver cycle, as set with ``xset s TIMEOUT CYCLE``.

                This can be used to run a countdown or (on laptops) dim the
                screen before locking. For an example, see the script
                */usr/share/doc/xss-lock/dim-screen.sh*.

--ignore-sleep  Do not lock on suspend/hibernate.

-v, --verbose   Be verbose.

-q, --quiet     Be quiet.

-h, --help      Print help message and exit.

--version       Print version number and exit.

See also
========

**xset**\(1),
**systemd-logind.service**\(8)
