#!/bin/sh

min_brightness=0

get_brighness() {
}

set_brightness() {
}

trap "set_brightness $(get_brightness); exit" TERM

set_brightness $min_brightness
