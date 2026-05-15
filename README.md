<h1 align=center>qView</h1>

<p align=center>qView is an image viewer designed with minimalism and usability in mind.</p>

<h3 align=center>
    <a href="https://interversehq.com/qview/">Visit the website</a>
</h3>

<h4 align=center>
    <a href="https://interversehq.com/qview/download">Downloads</a> |
    <a href="https://interversehq.com/qview/changelog">Changelog</a> | <a href="https://interversehq.com/discord">Discord</a>
</h4>

<p align=center>
    <a href="https://interversehq.com/qview/download">
        <img alt="Downloads shield" src="https://img.shields.io/github/downloads/jurplel/qview/total?color=blue&style=flat-square">
    </a>
    <a href="https://aur.archlinux.org/packages/qview/">
        <img alt="AUR shield" src="https://img.shields.io/aur/version/qview?style=flat-square">
    </a>
    <a href="https://formulae.brew.sh/cask/qview">
        <img alt="Homebrew cask shield" src="https://img.shields.io/homebrew/cask/v/qview?style=flat-square">
    </a>
</p>

<p align=center>
    <img alt="Screenshot" src="https://interversehq.com/qview/assets/img/screenshot3.png">
</p>

## IPC

qView can open an opt-in JSON IPC socket with `--ipc-server`. See
[docs/ipc.md](docs/ipc.md) for the current file path query and a Zsh helper.

## Command-line input

qView can open multiple files and directories as one navigation sequence. It also accepts `-` to
read line-delimited paths from standard input. See [docs/cli-input.md](docs/cli-input.md).
When a requested file has been renamed with ntags, qView can recover the tagged path in the same
directory.

## Keybindings

Default file navigation shortcuts are documented in
[docs/keybindings.md](docs/keybindings.md).
Display options, including the optional background details HUD, are documented in
[docs/display.md](docs/display.md).
