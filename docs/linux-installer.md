## Easy Linux installer

[Download the Linux installer](https://github.com/rhamenator/soundcurrent-eq/releases/download/windows-preview-0.7.6/SoundCurrent-EQ-Linux-Installer.run). Save it, then run:

```bash
bash ~/Downloads/SoundCurrent-EQ-Linux-Installer.run
```

It identifies your distribution, verifies the selected package against a pinned SHA-256, and installs or updates through APT or DNF with administrator approval. Presets and profiles are retained; an application-menu icon is included. Use Quit before updating. GTK/KDE confirmation dialogs are used when Zenity or KDialog is available.

The installer installs EQ 0.7.6, including the current profile library and editor. It supports Debian/Ubuntu derivatives with compatible Qt 6.4+ and glibc, Fedora 44+, and RHEL 10-compatible systems including Rocky/AlmaLinux 10. Dependency availability still depends on enabled distribution repositories. Unsupported distributions or architectures are reported; Arch and openSUSE packages are not provided yet. Use `--dry-run` to see its selection, or `--download-only` to save a verified package.


For an offline installation, download the matching DEB or RPM from the same release and run:

```bash
bash SoundCurrent-EQ-Linux-Installer.run --package-file /path/to/package
```

The local package must match the same pinned SHA-256 as the online package. Dependencies still require distribution repositories unless they are already installed.

To uninstall, Quit the app and remove its package with `sudo apt remove soundcurrent-eq` or `sudo dnf remove soundcurrent-eq`. Your personal presets and profiles are retained for reinstallation.
