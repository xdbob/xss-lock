#compdef xss-lock

function _xss-lock_arguments {
    _arguments -S -s : $@ \
        '(-n --notifier)'{-n,--notifier=}'[set notification command]: : _command_names -e' \
        '(-l --transfer-sleep-lock)'{-l,--transfer-sleep-lock}'[pass sleep delay lock file descriptor to locker]' \
        '--ignore-sleep[do not lock on suspend/hibernate]' \
        '(-q --quiet -v --verbose)'{-q,--quiet}'[output only fatal errors]' \
        '(-q --quiet -v --verbose)'{-v,--verbose}'[output more messages]' \
        '--version[print version number and exit]' \
        '(-h --help)'{-h,--help}'[print usage info and exit]'
}

function _arguments_after_locker {
    if (( complete_locker_args )); then
        _normal
    else
        _xss-lock_arguments
    fi
}

integer complete_locker_args=$(( words[(i)--] < CURRENT ))
_xss-lock_arguments \
    '(-):lock command: _command_names -e' \
    '*::arguments: _arguments_after_locker'
