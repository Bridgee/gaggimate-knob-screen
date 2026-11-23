"""Project the two display facade files with validation-only branches disabled."""
import re

def production_source(data):
    lines = data.splitlines(keepends=True)
    stack = []
    output = []
    for line in lines:
        text = line.strip()
        if text == b'#ifdef GAGGIMATE_KNOB_VALIDATION':
            stack.append([True, False]); continue
        if re.match(rb'#\s*(?:if|ifdef|ifndef)\b', text):
            stack.append([False, True])
        elif text == b'#else' and stack:
            if stack[-1][0]:
                stack[-1][1] = not stack[-1][1]; continue
        elif re.match(rb'#\s*endif\b', text) and stack:
            tagged, _ = stack.pop()
            if tagged: continue
        if all(not tagged or active for tagged, active in stack):
            output.append(line)
    if stack: raise RuntimeError('Unbalanced preprocessor guards')
    # Only blank lines/trailing whitespace are ignored; normal code/comments remain exact.
    return b'\n'.join(x.rstrip() for x in b''.join(output).splitlines() if x.strip())
