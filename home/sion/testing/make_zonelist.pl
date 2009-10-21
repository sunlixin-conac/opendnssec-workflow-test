#!/usr/bin/perl 
#===============================================================================
#
#         FILE:  make_zonelist.pl
#
#        USAGE:  ./make_zonelist.pl  
#
#  DESCRIPTION:  Make a zonelist with a configurable number of zones
#
#      OPTIONS:  ---
# REQUIREMENTS:  ---
#         BUGS:  ---
#        NOTES:  ---
#       AUTHOR:  YOUR NAME (), 
#      COMPANY:  
#      VERSION:  1.0
#      CREATED:  20/10/09 16:08:57
#     REVISION:  ---
#===============================================================================

use strict;
use warnings;

my $max = 10000;
my $prefix = "/home/sion/temp/opendnssec/install";

print "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
print "<ZoneList>\n";

foreach my $i (0 .. $max) {

    print "\t<Zone name=\"opendnssec$i.test\">\n";
	print "\t\t<Policy>default</Policy>\n";
	print "\t\t<SignerConfiguration>$prefix/var/opendnssec/config/opendnssec$i.test.xml</SignerConfiguration>\n";
	print "\t\t<Adapters>\n";
	print "\t\t\t<Input>\n";
	print "\t\t\t\t<File>$prefix/var/opendnssec/unsigned/opendnssec$i.test</File>\n";
	print "\t\t\t</Input>\n";
	print "\t\t\t<Output>\n";
	print "\t\t\t\t<File>$prefix/var/opendnssec/signed/opendnssec$i.test</File>\n";
	print "\t\t\t</Output>\n";
	print "\t\t</Adapters>\n";
	print "\t</Zone>\n";
}
print "</ZoneList>\n";
