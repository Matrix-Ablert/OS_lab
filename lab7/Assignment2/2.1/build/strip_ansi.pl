#!/usr/bin/perl
use strict;
use warnings;

# strip_ansi.pl - Strip ANSI escape codes from QEMU curses output
# Usage: perl strip_ansi.pl input.txt > output_clean.txt

my $file = shift or die "Usage: $0 <input_ansi_file>\n";
open my $fh, '<', $file or die "Cannot open $file: $!\n";
undef $/;
$_ = <$fh>;
close $fh;

# Replace screen clear / cursor home with newlines (makes curses output readable)
s/\x1b\[H\x1b\[2J/\n=== screen ===\n/g;
s/\x1b\[H\x1b\[J/\n/g;
s/\x1b\[[0-9;]*[Hf]/\n/g;
s/\x1b\[[0-9;]*d/\n/g;

# Strip ALL CSI sequences: ESC [ (params...) (final byte A-Za-z)
s/\x1b\[[\?0-9;]*[a-zA-Z]//g;

# Strip character set designations: ESC ( X  or  ESC ) X
s/\x1b[()][0-9a-zA-Z]//g;

# Strip mode set/reset: ESC =  and  ESC >
s/\x1b[=\x3e]//g;

# Strip OSC sequences (ESC ] ... BEL  or  ESC ] ... ESC \)
s/\x1b\][^\x07\x1b]*(\x07|\x1b\\)?//g;

# Strip other two-byte escapes (DCS, SOS, etc)
s/\x1b[PX^_].*?(\x1b\\)?//g;

# Normalize line endings
s/\r\n?/\n/g;

# Remove script header/footer
s/^Script started on.*\n//m;
s/^Script done on.*\n//m;

# Remove other control chars (keep tab and newline)
s/[\x00-\x08\x0b\x0c\x0e-\x1f]//g;

# Remove empty lines and deduplicate
my @lines = split /\n/;
my %seen;
my @out;
for my $line (@lines) {
    next if $line =~ /^\s*$/;
    next if $line =~ /^=== screen ===$/ && @out && $out[-1] =~ /^=== screen ===$/;
    next if $seen{$line}++;
    push @out, $line;
}
print join("\n", @out), "\n";
