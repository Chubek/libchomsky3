# Satie bash completions.
# Install: source this file, or copy to /etc/bash_completion.d/satie

_satie_cli_completion() {
  local cur prev opts
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"

  if [[ ${prev} == "--engine" ]]; then
    COMPREPLY=( $(compgen -W "native dpll cdcl" -- "${cur}") )
    return 0
  fi

  opts="--help --engine --model"
  if [[ ${cur} == -* ]]; then
    COMPREPLY=( $(compgen -W "${opts}" -- "${cur}") )
    return 0
  fi

  COMPREPLY=( $(compgen -f -- "${cur}") )
}
complete -F _satie_cli_completion satie-cli
complete -F _satie_cli_completion satie-repl
