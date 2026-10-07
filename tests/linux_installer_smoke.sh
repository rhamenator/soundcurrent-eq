#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/scripts/install-linux.run"
for distro in ubuntu debian linuxmint pop neon; do
    select_package "$distro" 'ubuntu debian' 24.04
    [[ $manager == apt-get && $filename == *.deb && $expected =~ ^[a-f0-9]{64}$ ]]
done
# RHEL advertises Fedora ancestry; it must select the enterprise build.
for distro in rhel rocky almalinux centos; do
    select_package "$distro" 'rhel centos fedora' 10.0
    [[ $manager == dnf && $filename == *.el10.x86_64.rpm ]]
done
select_package fedora '' 44
[[ $filename == *.fc44.x86_64.rpm ]]
! select_package fedora '' 43
! select_package rocky 'rhel centos fedora' 9
! select_package arch '' rolling
printf 'PASS: Debian derivatives, Fedora cutoff, RHEL ancestry and unsupported distro rejection\n'
