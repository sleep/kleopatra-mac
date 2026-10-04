#!/usr/bin/perl
# SPDX-FileCopyrightText: 2026 Kleopatra contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Adapts the checksum definitions of the libkleopatrarc of Libkleo to macOS.
# Usage: adapt-libkleopatrarc.pl <input file> <output file>

use strict;
use warnings;

my ($input, $output) = @ARGV;
open(my $in, '<', $input) or die "Cannot read $input: $!";
local $/;
my $config = <$in>;
close($in);

# The sha1sum, sha256sum and sha512sum tools don't exist on all supported versions of
# macOS, but shasum does. It writes and checks the same file format.
$config =~ s/^((?:create|verify)-command=.*?)\bsha(1|256|512)sum\b/$1shasum -a $2/mg;

# No tool that checks files with MD5 checksums exists on all supported versions of macOS.
# A definition with a missing tool is reported as error by the self-test, so it's removed.
$config =~ s/^\[Checksum Definition #\d+\]\n(?:(?!\[).*\n)*?id=md5sum\n(?:(?!\[).*\n)*//m;

open(my $out, '>', $output) or die "Cannot write $output: $!";
print $out $config;
close($out);
