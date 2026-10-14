# Satie fish shell completions.
# Install: copy to ~/.config/fish/completions/satie.fish

complete -c satie-cli -s h -l help    -d "Show help"
complete -c satie-cli -l engine       -d "Solver backend" -x -a "native dpll cdcl"
complete -c satie-cli -l model        -d "Print satisfying model"
complete -c satie-cli -a "(__fish_complete_path)" -d "Input file"
