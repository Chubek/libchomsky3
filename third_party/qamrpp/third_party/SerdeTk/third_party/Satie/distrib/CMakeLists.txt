# Distribution assets — each family is opt-in via an INSTALL_* option.
# Assets are data-only; nothing is compiled here.

option(INSTALL_FISH    "Install Fish shell completions"  OFF)
option(INSTALL_ZSH     "Install Zsh shell completions"   OFF)
option(INSTALL_BASH    "Install Bash shell completions"  OFF)
option(INSTALL_VIM     "Install Vim/Neovim integration"  OFF)
option(INSTALL_LSP     "Install LSP scaffold"            OFF)
option(INSTALL_SUBLIME "Install Sublime Text syntax"     OFF)

set(SATIE_SHARE ${CMAKE_INSTALL_DATADIR}/satie)

if(INSTALL_FISH)
  install(FILES fish/satie.fish    DESTINATION ${SATIE_SHARE}/fish)
endif()
if(INSTALL_ZSH)
  install(FILES zsh/_satie         DESTINATION ${SATIE_SHARE}/zsh)
endif()
if(INSTALL_BASH)
  install(FILES bash/satie.bash    DESTINATION ${SATIE_SHARE}/bash)
endif()
if(INSTALL_VIM)
  install(DIRECTORY vim/           DESTINATION ${SATIE_SHARE}/vim)
endif()
if(INSTALL_LSP)
  install(DIRECTORY lsp/           DESTINATION ${SATIE_SHARE}/lsp)
endif()
if(INSTALL_SUBLIME)
  install(FILES sublime-syntax/satie.sublime-syntax
          DESTINATION ${SATIE_SHARE}/sublime-syntax)
endif()

# Convenience meta-target: copy all distrib assets into the build tree.
add_custom_target(satie-distrib
  COMMAND ${CMAKE_COMMAND} -E echo
    "Use INSTALL_FISH/ZSH/BASH/VIM/LSP/SUBLIME=ON to install distribution assets.")
