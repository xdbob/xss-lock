#!/bin/bash

# Example notifier script -- sets brightness to $min_brightness, then waits to
# be killed and restores previous brightness on exit.

min_brightness=0

get_brightness() {
    xbacklight -get

    # Or, for drivers without RandR backlight property support (e.g. radeon):
    #cat /sys/class/backlight/acpi_video0/brightness
}

set_brightness() {
    xbacklight -set $1

    # Or, for drivers without RandR backlight property support (e.g. radeon):
    #echo $1 > /sys/class/backlight/acpi_video0/brightness
    #
    # To make this work, create a .conf file in /etc/tmpfiles.d/ containing the
    # following line to make the sysfs file writable for group 'users' (change
    # the last - to a number to set the initial brightness):
    #
    #     f /sys/class/backlight/acpi_video0/brightness 0664 root users - -
}

# To get a fading effect, replace one or both occurrences of set_brightness
# below with fade_brightness. Note that xbacklight natively supports fading.
fade_brightness() {
    local level
    for level in $(eval echo {$(get_brightness)..$1}); do
        set_brightness $level
        sleep 0.05
    done
}

trap 'exit 0' TERM
trap "set_brightness $(get_brightness); kill %%" EXIT
set_brightness $min_brightness
sleep 2147483647 &
wait
