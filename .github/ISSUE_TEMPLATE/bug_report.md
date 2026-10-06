---
name: Bug report
about: broseat misreports a seat, session, unit or inhibitor, fails to open a device, launches wrongly, hangs, or crashes
labels: bug
---

**What you called** (the smallest program or call sequence you can manage):

```cpp
```

**What the system says** (the independent view: `loginctl show-session`,
`loginctl seat-status`, `systemctl --user show-environment` / `status <unit>`,
`systemd-inhibit --list`, `busctl`):

```
```

**What broseat reports or does instead** (the error string, the
`LaunchResult`, the event sequence, a crash or hang; paste it):

```
```

**Does it reproduce in the tests?** Which `ctest` test fails, if any:

**Environment:**
- Distribution and version, systemd version:
- Where it ran (a VT, a desktop session and which compositor, ssh, a service):
- seatd / libseat version, if used:
- broseat commit:
