#!/usr/bin/perl
#
# $Id$

use XML::Simple;
use Data::Dumper;

print Dumper(XMLin(shift @ARGV));
