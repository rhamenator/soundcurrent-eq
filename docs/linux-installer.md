## Easy Linux installer

[Download the Linux installer](https://github.com/rhamenator/soundcurrent-eq/releases/download/windows-preview-0.7.6/SoundCurrent-EQ-Linux-Installer.run). Save it, then run:

```bash
bash ~/Downloads/SoundCurrent-EQ-Linux-Installer.run
```

It identifies your distribution, verifies the selected package against a pinned SHA-256, and installs or updates through APT or DNF with administrator approval. Presets and profiles are retained; an application-menu icon is included. Use Quit before updating. GTK/KDE confirmation dialogs are used when Zenity or KDialog is available.

The installer uses the published Linux preview (EQ 0.7.1), separate from the newer Windows build. It supports Debian/Ubuntu derivatives with compatible Qt 6.4+ and glibc, Fedora 44+, and RHEL 10-compatible systems including Rocky/AlmaLinux 10. Dependency availability still depends on enabled distribution repositories. Unsupported distributions or architectures are reported; Arch and openSUSE packages are not provided yet. Use `--dry-run` to see its selection, or `--download-only` to save a verified package.

