_xss-lock() {
    local cur prev words cword
    _init_completion || return

    local i notifier=@(-n|--notifier)
    for (( i=1; i <= COMP_CWORD; i++ )); do
        if [[ ${COMP_WORDS[i]} != -* ]]; then
            _command_offset $i
            return
        elif [[ ${COMP_WORDS[i]} == $notifier ]]; then
            (( i++ ))
        fi
    done

    if [[ $prev == $notifier ]]; then
        COMPREPLY=( $(compgen -c -- $cur) )
    fi

    if [[ $cur == -* ]]; then
        COMPREPLY=( $(compgen -W '-n --notifier -l --transfer-sleep-lock \
                                  --ignore-sleep -q --quiet -v --verbose \
                                  --version -h --help' -- $cur) )
    fi
}

complete -F _xss-lock xss-lock
