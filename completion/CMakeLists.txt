install(FILES xss-lock.zsh DESTINATION share/zsh/site-functions
        RENAME _xss-lock)
install(FILES xss-lock.bash DESTINATION share/bash-completion/completions
        RENAME xss-lock)
