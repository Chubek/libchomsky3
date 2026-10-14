# Vim / Neovim integration

Install the three files into your runtime path:

```
ftdetect/satie.vim   — registers *.satie, *.sat, *.cnf
syntax/satie.vim     — syntax highlighting
ftplugin/satie.vim   — commentstring + buffer defaults
```

Manual install (Vim):

```sh
cp ftdetect/satie.vim  ~/.vim/ftdetect/
cp syntax/satie.vim    ~/.vim/syntax/
cp ftplugin/satie.vim  ~/.vim/ftplugin/
```

For a plugin manager, add this repo's `distrib/vim` as a runtimepath entry.
