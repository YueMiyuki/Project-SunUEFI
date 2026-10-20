"""Stop helpers left inside a finished build root before unmounting its /dev."""
import os
from pathlib import Path
import select
import signal
import sys
import time


def stop_helpers(root):
    """Terminate only processes whose chroot is this build's root directory."""
    root = Path(root).resolve()
    if root == Path('/'):
        raise ValueError('Refusing to stop processes in the host root')
    pending = {}
    poller = select.poll()
    try:
        for proc in Path('/proc').glob('[0-9]*'):
            fd = None
            try:
                if not (proc / 'root').samefile(root):
                    continue
                fd = os.pidfd_open(int(proc.name))
                # The PID may have exited or been reused while opening it.
                if not (proc / 'root').samefile(root):
                    os.close(fd)
                    continue
                print(f'Stopping build helper {proc.name} in {root}', file=sys.stderr)
                signal.pidfd_send_signal(fd, signal.SIGTERM)
                poller.register(fd, select.POLLIN)
                pending[fd] = proc.name
            except (FileNotFoundError, ProcessLookupError, PermissionError):
                if fd is not None:
                    os.close(fd)
        # GnuPG's --kill command requests shutdown but need not wait for exit.
        # pidfds let us wait without confusing zombies or reused PIDs.
        for timeout, terminate in ((2000, False), (1000, True)):
            if not pending:
                break
            if terminate:
                for fd in pending:
                    try:
                        signal.pidfd_send_signal(fd, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            deadline = time.monotonic() + timeout / 1000
            while pending:
                remaining = max(0, int((deadline - time.monotonic()) * 1000))
                for fd, _ in poller.poll(remaining):
                    poller.unregister(fd)
                    os.close(fd)
                    del pending[fd]
                if time.monotonic() >= deadline:
                    break
        if pending:
            raise RuntimeError(f'Build helpers did not exit: {list(pending.values())}')
    finally:
        for fd in pending:
            os.close(fd)
