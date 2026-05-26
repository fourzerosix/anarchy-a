# Contributing to anarchy-s

No gods, no masters -- but pull requests are welcome.

## Setup

```bash
git clone https://github.com/fourzerosix/anarchy-s.git
cd anarchy-s
make
./anarchy-s
```

## Tests

```bash
make check
```

## Code style

- C99, no external dependencies beyond libc + libm
- Keep the single-file spirit of `src/anarchy-s.c`
- All terminal output degrades gracefully with `--plain`

## Ideas for flags

- `--proudhon` -- extremely slow, contemplative mode
- `--malatesta` -- draws the symbol three times in quick succession (prolific)
- `--haymarket` -- special historical date animation
- Multiple color themes
