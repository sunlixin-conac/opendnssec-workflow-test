#!/usr/bin/perl
use warnings;
use strict;
my $cmd ;

$cmd = "cd /tmp";

  if ($cmd =~ m/^cd\s/) {
    print substr($cmd, 2, len($cmd) - 3) . "\n";
    # chdir(substr($cmd, 2, len($cmd) - 3));
  } else {
    print "Assuming $cmd is a process!\n";
  }
