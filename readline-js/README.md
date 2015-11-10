# readline-js #
**readline-js** is a jQuery-based framework for emacs/readline-like
line editing commands. Future additions will include history and tab
completion, just like a real readline library.

# Using It #
Using readline-js is simple. Include the appropriate scripts in your
HTML document, and use the `readline` function on anything. **Only one
thing should be `readline`'d at a time or weird things could happen.**

Use of the readline function looks something like this:

```js
$('#my_text_entry').readline('myprompt> ', function(line) {
  alert('Line: ' + line);
});
```

The prompt argument is optional. If omitted, no prompt will be added to
the element. Note that all keypresses in the document will be sent to
the element.

## The HTML ##
readline-js adjusts the content HTML of the entry element. The prompt
will be contained within a `<span>` with the class `prompt`, and the
character under the cursor will be contained within a `<span>` with the
class `cursor`. The cursor `<span>` will also have any of the `ctrl` or
`shift` classes, depending on whether the corresponding modifiers are
being held down. This can be used to alter the appearance of the cursor
based on the status of the modifier keys. When blinking, the cursor's
class alternates between `cursor` and `cursor-blink`.

# Limitations and Quirks #
* Currently, only one element on the page may be used for text entry.
  This is a side effect of the focus model.
* All key events on the page are handled by readline-js (unless
  `readline()` was never used). This is because the `keydown` and `keyup`
  event handlers are attached to the `<body>` tag.
* Any sort of clipboard is entirely isolated from readline-js. This
  means that text cannot be pasted into the readline-js text entry field.
