fsv
===

A process supervisor.

`fsv` is a standalone process supervisor that is able to:

- start a process, restarting it if it crashes
- optionally attach another process to reliably log messages even if the main process is restarted
- maintain state information for all current `fsv`-managed processes at a per-user level

inspiration
-----------

`fsv` is inspired by `runit` and `daemontools`,
aiming to provide similar benefits without the need for service directories.

This will allow `fsv` to function as a drop-in process supervisor for
practically any init system.

building
--------

You need the `slog` library to build `fsv`.
If you have the `slog` project cloned into a directory right next to this one,
the build will pick it up automatically.
Otherwise, you will have to run `make SLOG=/path/to/slog`.

For example, my file tree looks like this:

```
~/
  git/
    fsv/
    slog/
```

To get a similar setup, you could run this:

```sh
cd
mkdir -p git && cd git
git clone https://github.com/hopkinsza/fsv.git
git clone https://github.com/hopkinsza/slog.git
```

Then, to build `slog` and then `fsv`:

```sh
cd slog
make
cd ../fsv
make
```

Note that you must run `make` in the `slog` project first to create `libslog.a`.
Otherwise, you will see a linker error like "slog not found".
