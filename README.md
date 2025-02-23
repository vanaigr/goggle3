# Goggle

A keyboard-first Linux app that lets you perform a Google search from anywhere using a keyboard shortcut:

![image](https://github.com/user-attachments/assets/35e13d01-50f9-41e6-9642-ee641982416a)

This is the third iteration of the same general concept. The previous two are:
* [Google refresh 2023](https://github.com/vanaigr/google-refresh-2023)
* [Goggle2](https://github.com/vanaigr/Goggle2)

This app is still in the prototyping phase. It works but is lacking some features i.e. definition results, featured answers, images, videos, favicons, etc.
It also only works with `i3`.

## Usage

Bind a key to open search:
```
bindsym <key> exec <Repo path>/show_search.sh
```

## Keyboard interface

There are 2 input modes: normal and insert.

Insert mode keys:
* [ctrl] left arrow - move cursor left [one word]
* [ctrl] right arrow - move cursor right [one word]
* Esc - exit insert mode
* ctrl backspace - delete word
* alt backspace - delete query

Normal mode keys:
* Yellow labels before result title - labels. Press corresponding key to open in the browser. First label is spacebar
* q - hide the window
* i - enter insert mode

Both modes:
* alt q - hide the window
* up arrow - scroll up
* down arrow - scroll down
* ctrl v - paste from clipboard
* Enter - search
